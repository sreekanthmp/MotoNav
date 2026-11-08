package com.motonav.navsdk

import android.app.Activity
import com.google.android.gms.maps.model.LatLng
import com.google.android.libraries.navigation.NavigationApi
import com.google.android.libraries.navigation.NavigationUpdatesOptions
import com.google.android.libraries.navigation.Navigator
import com.google.android.libraries.navigation.RoutingOptions
import com.google.android.libraries.navigation.SimulationOptions
import com.google.android.libraries.navigation.Waypoint

/**
 * Starts Google's navigation to a destination and registers the turn-by-turn feed, following the
 * sequence in Google's documentation: location permission, getNavigator (which asks for the
 * Navigation terms of use), setDestinations with the two-wheeler travel mode, then guidance.
 *
 * COMPILE-CHECKED ONLY: it has never run. It needs a Google Maps Platform API key with the
 * Navigation SDK enabled and billing on, location permission, and acceptance of Google's terms.
 *
 * Messages for the screen contain no road names and no coordinates.
 */
class NavigationController(
    private val activity: Activity,
    private val onMessage: (String) -> Unit,
) {
    private var navigator: Navigator? = null
    private var arrivalListener: Navigator.ArrivalListener? = null
    private var initialising = false

    val isReady: Boolean get() = navigator != null

    /** Asks the SDK for its Navigator. Shows Google's terms dialog if they were not accepted yet. */
    fun initialize() {
        if (navigator != null || initialising) return
        initialising = true
        onMessage("starting the Navigation SDK")
        NavigationApi.getNavigator(activity, object : NavigationApi.NavigatorListener {
            override fun onNavigatorReady(ready: Navigator) {
                initialising = false
                navigator = ready
                onMessage("Navigation SDK ready")
            }

            override fun onError(errorCode: Int) {
                initialising = false
                onMessage(describeError(errorCode))
            }
        })
    }

    /**
     * Routes to a point and starts guidance. With [simulate], the SDK moves a pretend vehicle
     * along the route at five times speed, so no driving is needed. [startFrom] (latitude,
     * longitude), if given, is where the pretend vehicle starts, so no GPS fix is needed either;
     * without it the route starts from the phone's real position.
     */
    fun start(latitude: Double, longitude: Double, simulate: Boolean, startFrom: Pair<Double, Double>? = null) {
        val nav = navigator
        if (nav == null) {
            onMessage("the Navigation SDK is not ready yet")
            return
        }
        NavFeed.onNewRoute()
        if (simulate && startFrom != null) nav.simulator.setUserLocation(LatLng(startFrom.first, startFrom.second))
        val destination = Waypoint.builder().setLatLng(latitude, longitude).setTitle("Destination").build()
        val options = RoutingOptions().travelMode(RoutingOptions.TravelMode.TWO_WHEELER)
        onMessage("asking Google for a two-wheeler route")
        // The variant with a RouteDescriptionLoadCallback; the older ones are deprecated by Google.
        nav.setDestinations(listOf(destination), options) { description ->
            val status = description.routeStatus
            if (status != Navigator.RouteStatus.OK) {
                onMessage("no route: $status")
            } else {
                beginGuidance(nav, simulate)
            }
        }
    }

    /** Stops guidance and the feed. */
    fun stop() {
        val nav = navigator ?: return
        arrivalListener?.let { nav.removeArrivalListener(it) }
        arrivalListener = null
        nav.unregisterServiceForNavUpdates()
        nav.stopGuidance()
        nav.clearDestinations()
        onMessage("navigation stopped")
    }

    private fun beginGuidance(nav: Navigator, simulate: Boolean) {
        val listener = Navigator.ArrivalListener { event ->
            if (event.isFinalDestination) {
                NavFeed.onArrival()
                onMessage("arrived at the final destination")
            }
        }
        arrivalListener?.let { nav.removeArrivalListener(it) }
        arrivalListener = listener
        nav.addArrivalListener(listener)

        // No step images, no preview steps: the device needs only the next maneuver.
        val updates = NavigationUpdatesOptions.builder()
            .setNumNextStepsToPreview(0)
            .setGeneratedStepImagesType(NavigationUpdatesOptions.GeneratedStepImagesType.NONE)
            .build()
        val registered = nav.registerServiceForNavUpdates(activity.packageName, NavUpdatesService::class.java.name, updates)
        onMessage(if (registered) "turn-by-turn feed registered" else "the turn-by-turn feed could not be registered")

        if (simulate) {
            nav.simulator.simulateLocationsAlongExistingRoute(SimulationOptions().speedMultiplier(5f))
        }
        nav.startGuidance()
        onMessage(if (simulate) "guidance started (simulated drive, 5x speed)" else "guidance started")
    }

    private fun describeError(code: Int): String = when (code) {
        NavigationApi.ErrorCode.NOT_AUTHORIZED ->
            "the Navigation SDK refused the API key (missing, wrong, or not enabled for Navigation)"
        NavigationApi.ErrorCode.TERMS_NOT_ACCEPTED -> "the Navigation terms of use were not accepted"
        NavigationApi.ErrorCode.NETWORK_ERROR -> "network error starting the Navigation SDK"
        NavigationApi.ErrorCode.LOCATION_PERMISSION_MISSING -> "location permission is missing"
        else -> "the Navigation SDK failed to start (error $code)"
    }
}
