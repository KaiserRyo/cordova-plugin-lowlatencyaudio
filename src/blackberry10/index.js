/*
* Copyright (c) 2013 BlackBerry Limited
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

var lowLatencyAudio;

module.exports = {

	preloadFX: function (success, fail, args, env) {
		var result = new PluginResult(args, env),
		    id = JSON.parse(unescape(args[0])),
		    assetPath = JSON.parse(unescape(args[1])),
		    response = lowLatencyAudio.getInstance().preloadFX(id, assetPath);
		result.ok(response, false);
	},

	preloadAudio: function (success, fail, args, env) {
		var result = new PluginResult(args, env),
		    id = JSON.parse(unescape(args[0])),
		    assetPath = JSON.parse(unescape(args[1])),
		    volume = args[2],
		    voices = args[3],
		    response = lowLatencyAudio.getInstance().preloadAudio(id, assetPath, volume, voices);
		result.ok(response, false);
	},

	play: function (success, fail, args, env) {
		var result = new PluginResult(args, env),
		    id = JSON.parse(unescape(args[0])),
		    response = lowLatencyAudio.getInstance().play(id);
		result.ok(response, false);
	},

	stop: function (success, fail, args, env) {
		var result = new PluginResult(args, env),
		    id = JSON.parse(unescape(args[0])),
		    response = lowLatencyAudio.getInstance().stop(id);
		result.ok(response, false);
	},

	loop: function (success, fail, args, env) {
		var result = new PluginResult(args, env),
		    id = JSON.parse(unescape(args[0])),
		    response = lowLatencyAudio.getInstance().loop(id);
		result.ok(response, false);
	},

	unload: function (success, fail, args, env) {
		var result = new PluginResult(args, env),
		    id = JSON.parse(unescape(args[0])),
		    response = lowLatencyAudio.getInstance().unload(id);
		result.ok(response, false);
	}

};

///////////////////////////////////////////////////////////////////
// JavaScript wrapper for JNEXT plugin for connection
///////////////////////////////////////////////////////////////////

JNEXT.LowLatencyAudio = function () {
	var self = this,
		hasInstance = false;

	self.getId = function () {
		return self.m_id;
	};

	self.init = function () {
		if (!JNEXT.require("libLowLatencyAudio")) {
			return false;
		}

		self.m_id = JNEXT.createObject("libLowLatencyAudio.LowLatencyAudio_JS");

		if (self.m_id === "") {
			return false;
		}

		JNEXT.registerEvents(self);
	};

	self.preloadFX = function (id, assetPath) {
		return JNEXT.invoke(self.m_id, "preloadFX " + id + " " + assetPath);
	};
	self.preloadAudio = function (id, assetPath, volume, voices) {
		return JNEXT.invoke(self.m_id, "preloadAudio " + id + " " + assetPath + " " + volume + " " + voices);
	};
	self.play = function (id) {
		return JNEXT.invoke(self.m_id, "play " + id);
	};
	self.stop = function (id) {
		return JNEXT.invoke(self.m_id, "stop " + id);
	};
	self.loop = function (id) {
		return JNEXT.invoke(self.m_id, "loop " + id);
	};
	self.unload = function (id) {
		return JNEXT.invoke(self.m_id, "unload " + id);
	};

	self.m_id = "";

	self.getInstance = function () {
		if (!hasInstance) {
			hasInstance = true;
			self.init();
		}
		return self;
	};

};

lowLatencyAudio = new JNEXT.LowLatencyAudio();
