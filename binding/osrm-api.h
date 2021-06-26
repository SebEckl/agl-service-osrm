/*
 * Copyright 2019 Konsulko Group
 * Author: Matt Ranostay <matt.ranostay@konsulko.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NAVIGATION_API_H
#define NAVIGATION_API_H

#include <stddef.h>

#define _GNU_SOURCE
#include <glib.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <glib-object.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

#include <json-c/json.h>

struct call_work;

struct navigation_state {
	// events
	afb_event_t destination_event;
	afb_event_t position_event;

	// storage
	json_object *destination_storage;
	json_object *position_storage;

	// locking
	GRWLock rw_lock;
};

#endif
