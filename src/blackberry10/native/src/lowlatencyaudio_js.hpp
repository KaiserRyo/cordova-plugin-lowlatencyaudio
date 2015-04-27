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

#ifndef LowLatencyAudio_JS_HPP_
#define LowLatencyAudio_JS_HPP_

#include <string>
#include "../public/plugin.h"
#include <qstring.h>
#include <qhash.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alut.h>
#include <vorbis/vorbisfile.h>

// #define SOUNDMANAGER_MAX_NBR_OF_SOURCES 32

class LowLatencyAudio_JS: public JSExt {

public:
    explicit LowLatencyAudio_JS(const std::string& id);
    virtual ~LowLatencyAudio_JS();
    std::string preloadFX(QString id, QString assetPath);
    std::string preloadAudio(QString id, QString assetPath, double volume, int voices);
    std::string play(QString id);
    std::string stop(QString id);
    std::string loop(QString id);
    std::string unload(QString id);
    virtual bool CanDelete();
    virtual std::string InvokeMethod(const std::string& command);

private:
    std::string m_id;

    // Load audio file based on it's type
    bool loadAudio(QString id, QString assetPath);
    // Load the .wav file
    bool loadWav(FILE* file, ALuint buffer);
    // Load the .ogg file
    bool loadOgg(FILE* file, ALuint buffer);

    QHash<QString, ALuint> m_audioBuffers;

    QHash<QString, ALuint> m_soundSources;
    QHash<QString, ALuint> m_assetSources;

};

#endif /* LowLatencyAudio_JS_HPP_ */
