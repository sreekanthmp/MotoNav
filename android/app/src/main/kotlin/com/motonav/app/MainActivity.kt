package com.motonav.app

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.pm.PackageManager
import android.graphics.Typeface
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.TypedValue
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.TextView
import com.motonav.link.Clock
import com.motonav.link.LinkListener
import com.motonav.link.LinkSession
import com.motonav.link.LinkState
import com.motonav.protocol.Scenarios
import com.motonav.protocol.StatusReport

/**
 * A minimal screen for testing the link to a MotoNav with simulated navigation (Phase 3).
 * It is not the product UI. COMPILE-CHECKED ONLY: never run on a phone.
 *
 * Nothing is stored. Road names appear on screen only, and nothing is written to the system log
 * or to disk (DECISIONS.md D-05); a test checks that this source never uses android.util.Log.
 */
// SetTextI18n: this is an English-only developer test screen, not the product UI.
@SuppressLint("SetTextI18n")
class MainActivity : Activity(), LinkListener, DeviceScanner.Listener {

    private val main = Handler(Looper.getMainLooper())
    private val clock = Clock { SystemClock.elapsedRealtime() }

    private var scanner: DeviceScanner? = null
    private var port: AndroidGattPort? = null
    private var session: LinkSession? = null

    private var playing = false
    private var nextScenario = 0
    private val log = ArrayDeque<String>()

    private lateinit var stateView: TextView
    private lateinit var statusView: TextView
    private lateinit var logView: TextView
    private lateinit var connectButton: Button
    private lateinit var disconnectButton: Button
    private lateinit var sendButton: Button
    private lateinit var playButton: Button
    private lateinit var wrongButton: Button
    private lateinit var scenarioSpinner: Spinner
    private lateinit var wrongSpinner: Spinner

    private val tick = object : Runnable {
        override fun run() {
            session?.tick()
            main.postDelayed(this, 1_000)
        }
    }

    private val playStep = object : Runnable {
        override fun run() {
            if (!playing) return
            val scenario = Scenarios.valid[nextScenario]
            nextScenario = (nextScenario + 1) % Scenarios.valid.size
            session?.submit(scenario.update)
            addLog("queued: ${scenario.name}")
            main.postDelayed(this, 1_000)
        }
    }

    // ---- screen ----

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        title = "MotoNav"

        val column = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(16), dp(16), dp(16))
        }
        val root = ScrollView(this).apply {
            addView(column, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            // Android 15 and later draw apps edge to edge: keep the content clear of the system bars.
            setOnApplyWindowInsetsListener { view, insets ->
                if (Build.VERSION.SDK_INT >= 30) {
                    val bars = insets.getInsets(WindowInsets.Type.systemBars())
                    view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
                } else {
                    @Suppress("DEPRECATION")
                    view.setPadding(insets.systemWindowInsetLeft, insets.systemWindowInsetTop,
                        insets.systemWindowInsetRight, insets.systemWindowInsetBottom)
                }
                insets
            }
        }
        setContentView(root)

        column.addView(text("MotoNav test sender", 22f, bold = true))
        column.addView(text("Phase 3: sends simulated navigation to the device. Not a navigation app.", 13f))

        stateView = text("", 16f, bold = true).apply { setPadding(0, dp(12), 0, dp(4)) }
        column.addView(stateView)
        connectButton = button("Find and connect") { findAndConnect() }
        disconnectButton = button("Disconnect") { session?.stop() }
        column.addView(connectButton)
        column.addView(disconnectButton)

        column.addView(section("Send a simulated state"))
        scenarioSpinner = spinner(Scenarios.valid.map { "${it.name}: ${it.description}" })
        column.addView(scenarioSpinner)
        sendButton = button("Send this state") {
            session?.submit(Scenarios.valid[scenarioSpinner.selectedItemPosition].update)
            addLog("queued: ${Scenarios.valid[scenarioSpinner.selectedItemPosition].name}")
        }
        column.addView(sendButton)
        playButton = button("Play all, one a second") { togglePlay() }
        column.addView(playButton)

        column.addView(section("Wrong messages (the device must reject each and change nothing)"))
        val wrong = Scenarios.invalid()
        wrongSpinner = spinner(wrong.map { "${it.name}: ${it.description}" })
        column.addView(wrongSpinner)
        wrongButton = button("Send this wrong message") {
            val chosen = wrong[wrongSpinner.selectedItemPosition]
            val sent = session?.sendRaw(chosen.frame) == true
            addLog(if (sent) "sent wrong message: ${chosen.name}" else "not sent (not ready, or a write is in flight)")
        }
        column.addView(wrongButton)

        column.addView(section("Device status"))
        statusView = text("(none yet)", 14f)
        column.addView(statusView)

        column.addView(section("Log (kept in memory only)"))
        logView = text("", 12f).apply { typeface = Typeface.MONOSPACE; setTextIsSelectable(true) }
        column.addView(logView)

        render(LinkState.Idle)
        main.postDelayed(tick, 1_000)
    }

    override fun onDestroy() {
        main.removeCallbacks(tick)
        main.removeCallbacks(playStep)
        scanner?.cancel()
        session?.stop()
        port?.close()
        super.onDestroy()
    }

    // ---- connecting ----

    private fun requiredPermissions(): Array<String> =
        if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)

    private fun findAndConnect() {
        val missing = requiredPermissions().filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isNotEmpty()) {
            requestPermissions(missing.toTypedArray(), REQUEST_PERMISSIONS)
            return
        }
        val adapter: BluetoothAdapter? = getSystemService(BluetoothManager::class.java)?.adapter
        if (adapter == null || !adapter.isEnabled) {
            addLog("Bluetooth is off or missing. Turn it on and try again.")
            return
        }
        if (session?.isActive == true) {
            addLog("already connected or connecting")
            return
        }
        addLog("scanning for a MotoNav (up to ${DeviceScanner.TIMEOUT_MS / 1000} s)")
        stateView.text = "Scanning..."
        connectButton.isEnabled = false
        scanner = DeviceScanner(adapter, main).also { it.start(this) }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != REQUEST_PERMISSIONS) return
        if (grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
            findAndConnect()
        } else {
            addLog("Bluetooth permission was refused; the app cannot work without it.")
        }
    }

    override fun onFound(device: BluetoothDevice, rssi: Int) {
        addLog("found a MotoNav (signal $rssi dBm); connecting")
        val newPort = AndroidGattPort(applicationContext, device, main)
        val newSession = LinkSession(newPort, this, clock)
        newPort.session = newSession
        port?.close()
        port = newPort
        session = newSession
        newSession.start()
    }

    override fun onFinished(found: Boolean, error: String?) {
        if (found) return
        addLog(error ?: "no MotoNav found. Is it powered, advertising and close by?")
        render(session?.state ?: LinkState.Idle)
    }

    // ---- link events ----

    override fun onStateChanged(state: LinkState) {
        if (state !is LinkState.Ready && playing) togglePlay()
        render(state)
        addLog("link: ${describe(state)}")
    }

    override fun onFrameWritten(sequence: Int, frame: ByteArray) = addLog("device acknowledged frame $sequence")

    override fun onWriteFailed(sequence: Int) = addLog("write of frame $sequence failed")

    override fun onStatus(report: StatusReport) {
        statusView.text = report.toString()
    }

    override fun onMalformedStatus(size: Int) = addLog("ignored a status value of $size bytes (expected 8)")

    private fun render(state: LinkState) {
        stateView.text = describe(state)
        val ready = state is LinkState.Ready
        val active = session?.isActive == true
        connectButton.isEnabled = !active && scanner?.isScanning != true
        disconnectButton.isEnabled = active
        sendButton.isEnabled = true            // a state sent before the link is ready is queued and sent on connecting
        playButton.isEnabled = ready || playing
        wrongButton.isEnabled = ready
    }

    private fun describe(state: LinkState): String = when (state) {
        is LinkState.Idle -> "Not connected"
        is LinkState.Connecting -> "Connecting..."
        is LinkState.DiscoveringServices -> "Looking for the MotoNav service..."
        is LinkState.NegotiatingMtu -> "Negotiating the message size..."
        is LinkState.EnablingNotifications -> "Turning on status messages..."
        is LinkState.Ready -> "Connected and ready (MTU ${state.mtu})"
        is LinkState.Closing -> "Disconnecting..."
        is LinkState.Closed -> "Disconnected"
        is LinkState.Failed -> "FAILED: ${state.failure}"
    }

    private fun togglePlay() {
        playing = !playing
        playButton.text = if (playing) "Stop playing" else "Play all, one a second"
        main.removeCallbacks(playStep)
        if (playing) {
            nextScenario = 0
            main.post(playStep)
        }
    }

    // ---- small helpers ----

    private fun addLog(line: String) {
        if (log.size >= MAX_LOG_LINES) log.removeFirst()
        log.addLast(line)
        logView.text = log.joinToString("\n")
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    private fun text(content: String, sp: Float, bold: Boolean = false): TextView = TextView(this).apply {
        text = content
        setTextSize(TypedValue.COMPLEX_UNIT_SP, sp)
        if (bold) typeface = Typeface.DEFAULT_BOLD
    }

    private fun section(title: String): TextView = text(title, 15f, bold = true).apply { setPadding(0, dp(20), 0, dp(4)) }

    private fun button(label: String, onClick: () -> Unit): Button = Button(this).apply {
        text = label
        setOnClickListener { _: View -> onClick() }
    }

    private fun spinner(items: List<String>): Spinner = Spinner(this).apply {
        adapter = ArrayAdapter(this@MainActivity, android.R.layout.simple_spinner_dropdown_item, items)
    }

    private companion object {
        const val REQUEST_PERMISSIONS = 1
        const val MAX_LOG_LINES = 60
    }
}
