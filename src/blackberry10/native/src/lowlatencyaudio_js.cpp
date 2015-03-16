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

#include <QDir>
#include <QFileInfo>
#include <qdebug.h>

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <pthread.h>
#include <time.h>

#include "lowlatencyaudio_js.hpp"

using namespace std;

bool isUsed = false;

void *useCheck(void *x_void_ptr) {

    int *x_ptr = (int *)x_void_ptr;
    usleep(*x_ptr * 1000000);

    if (isUsed == false)
        alutExit();

    return NULL;

}

/**
 * Default constructor.
 */
LowLatencyAudio_JS::LowLatencyAudio_JS(const std::string& id) :
		m_id(id) {

	// Initialize the ALUT and creates OpenAL context on default device
    // Input 0,0 so it grabs the native device as default and creates the context automatically
    alutInit(0, 0);

}

/**
 * LowLatencyAudio_JS destructor.
 */
LowLatencyAudio_JS::~LowLatencyAudio_JS() {

    ALuint bufferID = 0;
    ALuint sources = 0;
    QString name;

    // Stop and unload all files before deleting the sources and buffers
    for (int assetIndex = 0; assetIndex < m_assetBufferHash.size(); assetIndex++) {
        name = m_assetBufferHash.key(assetIndex);
        unload(name);
    }

    for (int soundIndex = 0; soundIndex < m_soundBufferHash.size(); soundIndex++) {
        name = m_soundBufferHash.key(soundIndex);
        unload(name);
    }

    // Clear the QHash for sound buffer ID
    m_soundIndexHash.clear();
    m_streamIndexHash.clear();
    m_soundBufferHash.clear();
    m_assetBufferHash.clear();

    // Exit the ALUT
    alutExit();

}

/**
 * This method returns the list of objects implemented by this native
 * extension.
 */
char* onGetObjList() {

    static char name[] = "LowLatencyAudio_JS";
	return name;

}

/**
 * This method is used by JNext to instantiate the LowLatencyAudio_JS object when
 * an object is created on the JavaScript server side.
 */
JSExt* onCreateObject(const string& className, const string& id) {

    if (className == "LowLatencyAudio_JS") {
		return new LowLatencyAudio_JS(id);
	}

	return NULL;

}

/**
 * Method used by JNext to determine if the object can be deleted.
 */
bool LowLatencyAudio_JS::CanDelete() {

    return true;

}

string LowLatencyAudio_JS::preloadFX(QString id, QString assetPath) {

    QString fileLocation;
    char cwd[PATH_MAX];
    ALuint bufferID;
    ALuint source;

    // Check to see if the file has already been preloaded
    // Append the applications directory to the www's sound files directory.
    if (m_soundBufferHash[id])
        return "Already preloaded " + id.toStdString();

    // First, we get the complete application directory in which we will load sounds from then
    // we convert to QString since it is more convenient when working with directories.
    // Append the applications directory to the www's sound files path.
    getcwd(cwd, PATH_MAX);
    fileLocation = QString(cwd).append("/app/native/").append(assetPath);

    // Create OpenAL buffers from all files in the sound directory.
    QFileInfo fileInfo(fileLocation);
    const char* path = fileInfo.absoluteFilePath().toStdString().c_str();

    // File existence check
    if (!(fileInfo.isFile() && fileInfo.exists()))
        return "Not loaded. Could not find file " + id.toStdString();

    // Create the Unique Buffer ID from the path
    bufferID = alutCreateBufferFromFile(fileLocation.toStdString().c_str());

    // Create sources and buffers corresponding to each unique file name.
    m_soundBufferHash[id] = bufferID;

    // Generate the sources and store them into m_assetIndexHash, our hash, using insert hashing
    // Also assign the bufferID to the sources
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, bufferID);
    m_soundIndexHash.insertMulti(id, source);

    //this->~PGaudio();  calling destructor to kill this object, not used in this case
    int x = 3;
    pthread_t useCheck_thread;

    // Create a thread to check if the preloaded audio file if being used or not. If not, kill the audio device channel
    if(pthread_create(&useCheck_thread, NULL, useCheck, &x)) {
        fprintf(stderr, "Error creating thread\n");
        return NULL;
    }

    return "File: <" + id.toStdString() + "> is loaded";

}

string LowLatencyAudio_JS::preloadAudio(QString id, QString assetPath, double volume, int voices) {

    QString fileLocation;
    char cwd[PATH_MAX];
    ALuint bufferID;
    ALuint source;

    // Check to see if the file has already been preloaded
    // Append the applications directory to the www's sound files directory
    if (m_assetBufferHash[id])
        return "Already preloaded " + id.toStdString();

    // First, we get the complete application directory in which we will load sounds from then
    // we convert to QString since it is more convenient when working with directories.
    // Append the applications directory to the www's sound files path.
    getcwd(cwd, PATH_MAX);
    fileLocation = QString(cwd).append("/app/native/").append(assetPath);

    // Create OpenAL buffers from all files in the sound directory
    QFileInfo fileInfo(fileLocation);
    const char* path = fileInfo.absoluteFilePath().toStdString().c_str();

    // File existence check
    if (!(fileInfo.isFile() && fileInfo.exists()))
        return "Not loaded. Could not find file " + id.toStdString();

    // Create the Unique Buffer ID from the path
    bufferID = alutCreateBufferFromFile(fileLocation.toStdString().c_str());

    // Create sources and buffers corresponding to each unique file name.
    m_assetBufferHash[id] = bufferID;

    for (int i = 0; i < voices; i++) {
        // Generate the sources, one by one, and store them into m_streamIndexHash, our hash, using insert hashing
        // Also assign the bufferID to the sources
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, bufferID);
        alSourcef(source, AL_GAIN, volume);
        m_streamIndexHash.insertMulti(id, source);
    }

    //this->~PGaudio();  calling destructor to kill this object, not used in this case
    int x = 3;
    pthread_t useCheck_thread;

    // Create a thread to check if the preloaded audio file if being used or not. If not, kill the audio device channel
    if(pthread_create(&useCheck_thread, NULL, useCheck, &x)) {
        fprintf(stderr, "Error creating thread\n");
        return NULL;
    }

    return "File: <" + id.toStdString() + "> is loaded";

}

string LowLatencyAudio_JS::unload(QString id) {

    isUsed = true;

    // Unload audio file loaded via preloadFX
    if (m_soundBufferHash[id]) {

        // Stop all sources before unloading
        QList<ALuint> sources = m_soundIndexHash.values(id);
        for (int i = 0; i < sources.size(); ++i) {
            alSourceStop(sources.at(i));
            alDeleteSources(1, &sources.at(i));
        }

        // Get corresponding buffers, voices and sources from the unique id
        ALuint bufferID = m_soundBufferHash[id];

        // Delete sources and buffers
        alDeleteBuffers(1, &bufferID);
        m_soundIndexHash.remove(id);

        // Re-initialize the buffers
        m_soundBufferHash[id] = 0;

        alutExit();

        return "Unloading " + id.toStdString();
    }

    // Unload audio file loaded via preloadAudio
    if (m_assetBufferHash[id]) {

        // Stop all sources before unloading
        stop(id);

        // Get corresponding buffers, voices and sources from the unique id
        ALuint bufferID = m_assetBufferHash[id];

        // Delete sources and buffers
        alDeleteBuffers(1, &bufferID);
        m_streamIndexHash.remove(id);

        // Re-initialize the buffers
        m_assetBufferHash[id] = 0;

        alutExit();

        return "Unloading " + id.toStdString();
    }

    return "Unloading Error: file not found <" + id.toStdString() + ">!";

}

string LowLatencyAudio_JS::stop(QString id){

    isUsed = true;

    // Stop audio file loaded via preloadFX
    if (m_soundBufferHash[id]) {
        return "Voices loaded via preloadFX are not intended to be looped or stopped.";
    }

    if (m_assetBufferHash[id]) {
        // Loop and stop every sound with corresponding file name
        QList<ALuint> sources = m_streamIndexHash.values(id);
        for (int i = 0; i < sources.size(); ++i)
            alSourceStop(sources.at(i));

        // Stopped playing source
        return "Stopped " + id.toStdString();
    }

    return "Stopping Error: file not found <" + id.toStdString() + ">!";

}

string LowLatencyAudio_JS::play(QString id){

    isUsed = true;

    float currentTime = 0, furthestTime = 0;
    ALuint replayingSource = 0;

    if (m_soundBufferHash[id]) {
        // Get corresponding buffer from the unique id
        ALuint bufferID = m_soundBufferHash[id];

        // Check to see if it has been preloaded
        if (!bufferID)
            return "Could not find the file " + id.toStdString() + " . Maybe it hasn't been loaded.";

        // Iterate through a list of sources associated with with the file and play the sound if it is not currently playing
        QList<ALuint> sources = m_soundIndexHash.values(id);
        for (int i = 0; i < sources.size(); ++i) {
            ALenum state;
            alGetSourcef(sources.at(i), AL_SEC_OFFSET, &currentTime);
            alGetSourcei(sources.at(i), AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING) {
                alSourcePlay(sources.at(i));
                return "Playing " + id.toStdString();
            }
            if (currentTime > furthestTime) {
                furthestTime = currentTime;
                replayingSource = sources.at(i);
            }
        }
    }

    if (m_assetBufferHash[id]) {
        // Get corresponding buffer from the unique id
        ALuint bufferID = m_assetBufferHash[id];

        // Check to see if it has been preloaded
        if (!bufferID)
            return "Could not find the file " + id.toStdString() + " . Maybe it hasn't been loaded.";

        // Iterate through a list of sources associated with with the file and play the sound if it is not currently playing
        QList<ALuint> sources = m_streamIndexHash.values(id);
        for (int i = 0; i < sources.size(); ++i) {
            ALenum state;
            alGetSourcef(sources.at(i), AL_SEC_OFFSET, &currentTime);
            alGetSourcei(sources.at(i), AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING) {
                alSourcePlay(sources.at(i));
                return "Playing " + id.toStdString();
            }
            if (currentTime > furthestTime) {
                furthestTime = currentTime;
                replayingSource = sources.at(i);
            }
        }
    }

    // Continue cycling through and overwrite the sources if all sources are currently being used
    alSourcePlay(replayingSource);
    return "Every single voice is currently being played, now overwriting previous ones";

}

string LowLatencyAudio_JS::loop(QString id){

    isUsed = true;

    // Loop audio file loaded via preloadFX
    if (m_soundBufferHash[id]) {
        return "Voices loaded via preloadFX are not intended to be looped or stopped.";
    }

    // Loop audio file loaded via preloadAudio
    if (m_assetBufferHash[id]) {
        // Get corresponding buffers and sources from the unique id
        ALuint bufferID = m_assetBufferHash[id];

        // If sound file has been preloaded loop sound
        if (!bufferID)
            return "Could not find the file " + id.toStdString() + " . Maybe it hasn't been loaded.";

        // Get the source from which the sound will be played
        ALuint source;
        QList<ALuint> sources = m_streamIndexHash.values(id);
        source = sources.at(sources.size() - 1);

        if (alIsSource(source) == AL_TRUE) {

            // Loop the source
            alSourcei(source, AL_LOOPING, AL_TRUE);

            // Check to see if the sound is already looping or not, if not, loop sound
            ALenum state;
            alGetSourcei(source, AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING) {
                alSourcePlay(source);
                return "Looping " + id.toStdString();
            }

        }
        return id.toStdString() + " is already playing";
    }

    return "Could not find the file " + id.toStdString() + " . Maybe it hasn't been loaded.";
}

/**
 * It will be called from JNext JavaScript side with passed string.
 * This method implements the interface for the JavaScript to native binding
 * for invoking native code. This method is triggered when JNext.invoke is
 * called on the JavaScript side with this native objects id.
 */
string LowLatencyAudio_JS::InvokeMethod(const string& command) {

    // parse command and args from string
    int indexOfFirstSpace = command.find_first_of(" ");
    string strCommand = command.substr(0, indexOfFirstSpace);
    string strValue = command.substr(indexOfFirstSpace + 1, command.length());

    // Convert input file name from string to QString
    QString id = QString::fromStdString(strValue);

    // Determine which function should be executed

    // Loop with given fileName.
    if (strCommand == "preloadFX") {
        // parse id and path from strValue
        int indexOfSecondSpace = strValue.find_first_of(" ");
        string idString = strValue.substr(0, indexOfSecondSpace);
        string pathString = strValue.substr(indexOfSecondSpace + 1, strValue.length());

        QString id = QString::fromStdString(idString);
        QString assetPath = QString::fromStdString(pathString);

        return preloadFX(id, assetPath);
    }

    if (strCommand == "preloadAudio") {
        // parse id from strValue
        int indexOfSecondSpace = strValue.find_first_of(" ");
        string idString = strValue.substr(0, indexOfSecondSpace);
        string pathVolumeVoice = strValue.substr(indexOfSecondSpace + 1, strValue.length());

        // parse path from pathVolumeVoice
        int indexOfThirdSpace = pathVolumeVoice.find_first_of(" ");
        string pathString = pathVolumeVoice.substr(0, indexOfThirdSpace);
        string volumeVoice = pathVolumeVoice.substr(indexOfThirdSpace + 1, pathVolumeVoice.length());

        // parse volume and voices from volumeVoice
        int indexOfFourthSpace = volumeVoice.find_first_of(" ");
        string volumeString = volumeVoice.substr(0, indexOfFourthSpace);
        string voicesString = volumeVoice.substr(indexOfFourthSpace + 1, volumeVoice.length());

        QString id = QString::fromStdString(idString);
        QString assetPath = QString::fromStdString(pathString);
        double volume = atof (volumeString.c_str());
        int voices = atoi (voicesString.c_str());

        return preloadAudio(id, assetPath, volume, voices);
    }

    // Play with given fileName.
    if (strCommand == "play")
        return play(id);

    // Loops the source
    if (strCommand == "loop")
        return loop(id);

    // Unloads the source
    if (strCommand == "unload")
        return unload(id);

    // Stop the source.
    if (strCommand == "stop")
        return stop(id);

    return "Command not found, choose either: load, unload, play ,loop, or stop";

}
