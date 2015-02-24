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

class LowLatencyAudio_JS: public JSExt {

public:
    explicit LowLatencyAudio_JS(const std::string& id);
    virtual ~LowLatencyAudio_JS();
    virtual std::string preloadFX(QString id, QString assetPath);
    virtual std::string preloadAudio(QString id, QString assetPath, double volume, int voices);
    virtual std::string play(QString id);
    virtual std::string stop(QString id);
    virtual std::string loop(QString id);
    virtual std::string unload(QString id);
    virtual bool CanDelete();
    virtual std::string InvokeMethod(const std::string& command);
private:
    std::string m_id;
    QHash<QString, ALuint> m_soundBufferHash;
    QHash<QString, ALuint> m_soundIndexHash;
    QHash<QString, ALuint> m_assetBufferHash;
    QHash<QString, ALuint> m_streamIndexHash;
};

#endif /* LowLatencyAudio_JS_HPP_ */
