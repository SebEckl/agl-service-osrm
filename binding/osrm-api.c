/*
 * Dieses Dokument enthält keinen lauffähigen Code sondern lediglich Pseudocode.
 * Der Pseudocode soll als Hilfestellung für die künftige Entwicklung des Service dienen
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <json-c/json.h>


#include "curl/curl.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;


#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

#include "navigation-api.h"

afb_api_t g_api;

//get user data
struct navigation_state* navigation_get_userdata(void)
{
	return afb_api_get_userdata(g_api);
}

//get event type
static afb_event_t get_event_from_value(struct navigation_state* ns,
	const char* value)
{
	if (!g_strcmp0(value, "destination"))
		return ns->destination_event;

	if (!g_strcmp0(value, "position"))
		return ns->position_event;

	return NULL;
}

//get storage
static json_object** get_storage_from_value(struct navigation_state* ns,
	const char* value)
{
	if (!g_strcmp0(value, "destination"))
		return &ns->destination_storage;

	if (!g_strcmp0(value, "position"))
		return &ns->position_storage;

	return NULL;
}

//helper function to write GeoJSON
static size_t my_write(void* buffer, size_t size, size_t nmemb, void* param)
{
	std::string& text = *static_cast<std::string*>(param);
	size_t totalsize = size * nmemb;
	text.append(static_cast<char*>(buffer), totalsize);
	return totalsize;
}

//download_route() function from osrm-curl-test
static void download_route()
{
	struct navigation_state* ns = navigation_get_userdata();

	//get position and destination from storage
	json_object* position = get_storage_from_value(ns, "position");
	json_object* destination = get_storage_from_value(ns, "destination");

	//Build URL 
	//with local OSRM Server installed and started, url can be changed to: "http://localhost:5000/route/v1/driving/"+...
	std::string url = "http://router.project-osrm.org/route/v1/driving/" +
		position["points"][0]["longitude"].dump() + "," +
		position["points"][0]["latitude"].dump() + ";";

	for (int i = 0; i < destination["points"][0]["waypoints"].size(); i++) {
		url = url + destination["points"][0]["waypoints"][i]["longitude"].dump() + "," +
			destination["points"][0]["waypoints"][i]["latitude"].dump() + ";";
	}

	url = url + destination["points"][1]["destination"][0]["longitude"].dump() + "," +
		destination["points"][1]["destination"][0]["latitude"].dump() +
		"?steps=true";

	//get route with curl
	std::string result;
	CURL* curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (CURLE_OK != res) {
			std::cerr << "CURL error: " << res << '\n';
		}
	}
	curl_global_cleanup();

	auto jresult = json::parse(result);

	//broadcast route with broadcast_waypoints verb of navigation api
	afb_api_call_sync(api, "navigation", "broadcast_waypoints", jresult, &response, &err, &info);
}


// saves incoming events in local storage
static void handler(json_object* jresp, const char* name)
{
	struct navigation_state* ns = navigation_get_userdata();
	afb_event_t event = get_event_from_value(ns, "position");
	json_object* tmp = NULL;

	if (json_object_deep_copy(jresp, (json_object**)&tmp, NULL))
		return;

	json_object** storage;

	storage = get_storage_from_value(ns, "position");

	if (*storage)
		json_object_put(*storage);
	*storage = NULL;

	// increment reference for storage
	json_object_get(tmp);
	*storage = tmp;
	return;
}

//handle position event
static void handle_position(afb_req_t request)
{
	json_object* jresp = afb_req_json(request);
	handler(jresp, "position");
			
	//call download_route() if a navigation is active
	if (navigation_is_active) {
		download_route();
	}
}

//handle destination event
static void handle_destination(afb_req_t request)
{
	json_object* jresp = afb_req_json(request);
	handler(jresp, "destination");
		
	//call download_route() -> starts navigation
	download_route();	
}


//init function for binding
static int init(afb_api_t api)
{
	// initialize events
	ns->destiantion_event = afb_daemon_make_event("destination");
	ns->position_event = afb_daemon_make_event("position");

	if (!afb_event_is_valid(ns->destination_event) ||
		!afb_event_is_valid(ns->position_event) {
		AFB_ERROR("Cannot create events");
		return -EINVAL;
	}

	//get user data
	afb_api_set_userdata(api, ns);
	g_api = api;

	//subscribe to position and destination
	afb_api_call_sync(api, "navigation", "subscribe", "{'value':'position'}", &response, &err, &info);
	afb_api_call_sync(api, "navigation", "subscribe", "{'value':'destination'}", &response, &err, &info);

	//add event handlers for incoming position and destination events 
	static const afb_api_event_handler_add(api, "position", handle_position, void* closure);
	static const afb_api_event_handler_add(api, "destination", handle_destination, void* closure);

	g_rw_lock_init(&ns->rw_lock);

	return 0;
}

/*
 * description of the binding for afb-daemon
 */
const afb_binding_t afbBindingV3 = {
	.api = "osrm",
	.init = init,
};
