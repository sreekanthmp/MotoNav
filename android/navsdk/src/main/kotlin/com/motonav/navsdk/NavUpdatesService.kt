package com.motonav.navsdk

import android.app.Service
import android.content.Intent
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import com.google.android.libraries.mapsplatform.turnbyturn.TurnByTurnManager

/**
 * The service the Navigation SDK binds to and feeds about once a second (the "turn-by-turn feed").
 * It decodes each message and hands it on; it decides nothing.
 *
 * COMPILE-CHECKED ONLY: it has never received a message from the real SDK.
 *
 * Declared in this module's manifest, not exported, in the app's own process (the hand-off to the
 * link is in-process). The SDK starts it when [NavigationController] registers it and stops it
 * when it unregisters.
 */
class NavUpdatesService : Service() {
    private val turnByTurn: TurnByTurnManager = TurnByTurnManager.createInstance()
    private lateinit var messenger: Messenger

    override fun onCreate() {
        super.onCreate()
        messenger = Messenger(Handler(Looper.getMainLooper()) { message ->
            handle(message)
            true
        })
    }

    override fun onBind(intent: Intent?): IBinder = messenger.binder

    private fun handle(message: Message) {
        if (message.what != TurnByTurnManager.MSG_NAV_INFO) return
        val info = try {
            turnByTurn.readNavInfoFromBundle(message.data)
        } catch (_: RuntimeException) {
            null
        }
        if (info == null) NavFeed.onMalformedMessage() else NavFeed.onNavInfo(info)
    }
}
