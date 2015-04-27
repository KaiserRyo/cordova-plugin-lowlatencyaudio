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

// Error message function for ALUT.
static void reportALUTError(ALenum error)
{
    if (error != ALUT_ERROR_NO_ERROR)
        qDebug() << "ALUT reported the following error: " << alutGetErrorString(error);
}

// Error message function for OpenAL.
static void reportOpenALError(ALenum error)
{
    if (error != AL_NO_ERROR)
        qDebug() << "OpenAL reported the following error: \n" << alutGetErrorString(error);
}

bool LowLatencyAudio_JS::loadWav(FILE* file, ALuint buffer)
{
    unsigned char stream[12];

    // Verify the wave fmt magic value meaning format.
    if (fread(stream, 1, 8, file) != 8 || memcmp(stream, "fmt ", 4) != 0 )  {
        qDebug() << "Failed to verify the magic value for the wave file format.";
        return false;
    }

    unsigned int section_size;
    section_size  = stream[7]<<24;
    section_size |= stream[6]<<16;
    section_size |= stream[5]<<8;
    section_size |= stream[4];

    // Check for a valid pcm format.
    if (fread(stream, 1, 2, file) != 2 || stream[1] != 0 || stream[0] != 1) {
        qDebug() << "Unsupported audio file format (must be a valid PCM format).";
        return false;
    }

    // Get the channel count (16-bit little-endian).
    int channels;
    if (fread(stream, 1, 2, file) != 2) {
        qDebug() << "Failed to read the wave file's channel count.";
        return false;
    }
    channels  = stream[1]<<8;
    channels |= stream[0];

    // Get the sample frequency (32-bit little-endian).
    ALuint frequency;
    if (fread(stream, 1, 4, file) != 4) {
        qDebug() << "Failed to read the wave file's sample frequency.";
        return false;
    }

    frequency  = stream[3]<<24;
    frequency |= stream[2]<<16;
    frequency |= stream[1]<<8;
    frequency |= stream[0];

    // The next 6 bytes hold the block size and bytes-per-second.
    // We don't need that info, so just read and ignore it.
    // We could use this later if we need to know the duration.
    if (fread(stream, 1, 6, file) != 6) {
        qDebug() << "Failed to read past the wave file's block size and bytes-per-second.";
        return false;
    }

    // Get the bit depth (16-bit little-endian).
    int bits;
    if (fread(stream, 1, 2, file) != 2) {
        qDebug() << "Failed to read the wave file's bit depth.";
        return false;
    }
    bits  = stream[1]<<8;
    bits |= stream[0];

    // Now convert the given channel count and bit depth into an OpenAL format.
    ALuint format = 0;
    if (bits == 8) {
        if (channels == 1)
            format = AL_FORMAT_MONO8;
        else if (channels == 2)
            format = AL_FORMAT_STEREO8;
    }
    else if (bits == 16) {
        if (channels == 1)
            format = AL_FORMAT_MONO16;
        else if (channels == 2)
            format = AL_FORMAT_STEREO16;
    }
    else {
        qDebug() << "Incompatible wave file format: ( " << channels << ", " << bits << ")";
        return false;
    }

    // Check against the size of the format header as there may be more data that we need to read.
    if (section_size > 16) {
        unsigned int length = section_size - 16;

        // Extension size is 2 bytes.
        if (fread(stream, 1, length, file) != length) {
            qDebug() << "Failed to read extension size from wave file.";
            return false;
        }
    }

    // Read in the rest of the file a chunk (section) at a time.
    while (true) {
        // Check if we are at the end of the file without reading the data.
        if (feof(file)) {
            qDebug() << "Failed to load wave file; file appears to have no data.";
            return false;
        }

        // Read in the type of the next section of the file.
        if (fread(stream, 1, 4, file) != 4) {
            qDebug() << "Failed to read next section type from wave file.";
            return false;
        }

        // Data chunk.
        if (memcmp(stream, "data", 4) == 0) {
            // Read how much data is remaining and buffer it up.
            unsigned int dataSize;
            if (fread(&dataSize, sizeof(int), 1, file) != 1) {
                qDebug() << "Failed to read size of data section from wave file.";
                return false;
            }

            char* data = new char[dataSize];
            if (fread(data, sizeof(char), dataSize, file) != dataSize)  {
                qDebug() << "Failed to load wave file; file is missing data.";
                delete data;
                return false;
            }

            alBufferData(buffer, format, data, dataSize, frequency);
            ALenum error = alGetError();
            if (error != AL_NO_ERROR) {
                reportOpenALError(error);
            }
            delete data;

            // We've read the data, so return now.
            return true;
        }
        // Other chunk - could be any of the following:
        // - Fact ("fact")
        // - Wave List ("wavl")
        // - Silent ("slnt")
        // - Cue ("cue ")
        // - Playlist ("plst")
        // - Associated Data List ("list")
        // - Label ("labl")
        // - Note ("note")
        // - Labeled Text ("ltxt")
        // - Sampler ("smpl")
        // - Instrument ("inst")
        else {
            // Store the name of the chunk so we can report errors informatively.
            char chunk[5] = { 0 };
            memcpy(chunk, stream, 4);

            // Read the chunk size.
            if (fread(stream, 1, 4, file) != 4) {
                qDebug() << "Failed to read size of " << chunk << "chunk from wave file.";
                return false;
            }

            section_size  = stream[3]<<24;
            section_size |= stream[2]<<16;
            section_size |= stream[1]<<8;
            section_size |= stream[0];

            // Seek past the chunk.
            if (fseek(file, section_size, SEEK_CUR) != 0) {
                qDebug() << "Failed to seek past " << chunk << "in wave file.";
                return false;
            }
        }
    }

    return true;
}

bool LowLatencyAudio_JS::loadOgg(FILE* file, ALuint buffer)
{
    OggVorbis_File ogg_file;
    vorbis_info* info;
    ALenum format;
    int result;
    int section;
    unsigned int size = 0;

    rewind(file);

    if ((result = ov_open(file, &ogg_file, NULL, 0)) < 0) {
        fclose(file);
        qDebug() << "Failed to open ogg file.";
        return false;
    }

    info = ov_info(&ogg_file, -1);

    if (info->channels == 1)
        format = AL_FORMAT_MONO16;
    else
        format = AL_FORMAT_STEREO16;

    // size = #samples * #channels * 2 (for 16 bit).
    unsigned int data_size = ov_pcm_total(&ogg_file, -1) * info->channels * 2;
    char* data = new char[data_size];

    while (size < data_size) {
        result = ov_read(&ogg_file, data + size, data_size - size, 0, 2, 1, &section);
        if (result > 0) {
            size += result;
        }
        else if (result < 0) {
            delete data;
            qDebug() << "Failed to read ogg file; file is missing data.";
            return false;
        }
        else {
            break;
        }
    }

    if (size == 0) {
        delete data;
        qDebug() << "Filed to read ogg file; unable to read any data.";
        return false;
    }

    alBufferData(buffer, format, data, data_size, info->rate);

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        reportOpenALError(error);
    }

    delete data;
    ov_clear(&ogg_file);

    // ov_clear actually closes the file pointer as well.
    file = 0;

    return true;
}

bool LowLatencyAudio_JS::loadAudio(QString id, QString assetPath) {
    QString fileLocation;
    char cwd[PATH_MAX];
    ALuint bufferID;

    // Retrieve the actual file location
    getcwd(cwd, PATH_MAX);
    fileLocation = QString(cwd)
                    .append("/app/native/")
                    .append(assetPath);

    QFileInfo fileInfo(fileLocation);
    const char* path = fileInfo.absoluteFilePath().toStdString().c_str();

    // Generate buffers to hold audio data.
    alGenBuffers(1, &m_audioBuffers[id]);

    // Load sound file.
    FILE* file = fopen(path, "rb");

    // Read the file header
    char header[12];
    bufferID = m_audioBuffers[id];
    if (fread(header, 1, 12, file) != 12) {
        qDebug() << "Invalid header for audio file " << path;
        alDeleteBuffers(1, &bufferID);
        if (file) { fclose(file); }
        return false;
    }

    // Check the file format & load the buffer with audio data.
    if (memcmp(header, "RIFF", 4) == 0) {
        if (!loadWav(file, bufferID)) {
            qDebug() << "Invalid wav file: " << path;
            alDeleteBuffers(1, &bufferID);
            if (file) { fclose(file); }
            return false;
        }
    }
    else if (memcmp(header, "OggS", 4) == 0) {
        if (!loadOgg(file, bufferID)) {
            qDebug() << "Invalid ogg file: " << path;
            alDeleteBuffers(1, &bufferID);
            if (file) { fclose(file); }
            return false;
        }
    }
    else {
        qDebug() << "Unsupported audio file: " << path;
        if (file) { fclose(file); }
        return false;
    }

    return true;
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
    for (int bufferIndex = 0; bufferIndex < m_audioBuffers.size(); bufferIndex++) {
        name = m_audioBuffers.key(bufferIndex);
        unload(name);
    }

    m_assetSources.clear();
    m_soundSources.clear();
    m_audioBuffers.clear();

    // Exit the ALUT.
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
    ALuint bufferID;
    ALuint source;

    // Load the audio file into memory if necessary
    if (!m_audioBuffers[id]) {
        if (!loadAudio(id, assetPath))
            return "preloadFX failed: " + id.toStdString();
    }

    // Create sound source if not available
    if (!m_soundSources[id]) {
        bufferID = m_audioBuffers[id];
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, bufferID);
        m_soundSources.insertMulti(id, source);
    }

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
    ALuint bufferID;
    ALuint source;

    // Load the audio file into memory if necessary
    if (!m_audioBuffers[id]) {
        if (!loadAudio(id, assetPath))
            return "preloadAudio failed: " + id.toStdString();
    }

    // Create asset source if not available
    if (!m_soundSources[id]) {
        bufferID = m_audioBuffers[id];
        for (int i = 0; i < voices; i++) {
            alGenSources(1, &source);
            alSourcei(source, AL_BUFFER, bufferID);
            alSourcef(source, AL_GAIN, (float)(volume));
            m_assetSources.insertMulti(id, source);
        }
    }

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

    // Stop all sources before unloading.
    stop(id);

    // Get corresponding buffers, voices and sources from the unique file name.
    ALuint bufferID = m_audioBuffers[id];

    // Loop to make sure every source is deleted in case it had multiple voices.
    QList<ALuint> sources = m_soundSources.values(id);
    for (int i = 0; i < sources.size(); ++i)
        alDeleteSources(1, &sources.at(i));

    sources = m_assetSources.values(id);
    for (int i = 0; i < sources.size(); ++i)
        alDeleteSources(1, &sources.at(i));

    // Delete sources and buffers.
    alDeleteBuffers(1, &bufferID);
    m_soundSources.remove(id);
    m_assetSources.remove(id);

    // Re-initialize the buffers.
    m_audioBuffers[id] = 0;

    alutExit();

    return "Unloading " + id.toStdString();
}

// Function to stop playing sounds. Takes in sound file name.
string LowLatencyAudio_JS::stop(QString id){
    isUsed = true;

    // Loop and stop every sound with corresponding fileName.
    QList<ALuint> sources = m_soundSources.values(id);
    for (int i = 0; i < sources.size(); ++i)
        alSourceStop(sources.at(i));

    sources = m_assetSources.values(id);
    for (int i = 0; i < sources.size(); ++i)
        alSourceStop(sources.at(i));

    // Stopped playing source.
    return "Stopped " + id.toStdString();
}

// Function to play single sound. Takes in one parameter, the sound file name.
string LowLatencyAudio_JS::play(QString id){
    isUsed = true;

    float currentTime = 0, furthestTime = 0;
    ALuint replayingSource = 0;

    // Get corresponding buffer from the unique file name.
    ALuint bufferID = m_audioBuffers[id];

    // Check to see if it has been preloaded.
    if (!bufferID)
        return "Could not find the file " + id.toStdString() + " . Maybe it hasn't been loaded.";

    // Iterate through a list of sources associated with with the file and play the sound if it is not currently playing
    QList<ALuint> sources;
    if (m_soundSources[id]) {
        sources = m_soundSources.values(id);
    } else {
        sources = m_assetSources.values(id);
    }

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

    // Continue cycling through and overwrite the sources if all sources are currently being used
    alSourcePlay(replayingSource);
    return "Every single voice is currently being played, now overwriting previous ones";
}

// Function to loop sound.
string LowLatencyAudio_JS::loop(QString id){
    isUsed = true;

    // Get corresponding buffers and sources from the unique file name.
    ALuint bufferID = m_audioBuffers[id];

    // If sound file has been preloaded loop sound
    if (!bufferID)
        return "Could not find the file " + id.toStdString() + " . Maybe it hasn't been loaded.";

    // Get the source from which the sound will be played.
    QList<ALuint> sources;
    if (m_soundSources[id]) {
        sources = m_soundSources.values(id);
    } else {
        sources = m_assetSources.values(id);
    }

    ALuint source;
    source = sources.at(sources.size() - 1);

    if (alIsSource(source) == AL_TRUE) {
        // Loop the source.
        alSourcei(source, AL_LOOPING, AL_TRUE);

        // Check to see if the sound is already looping or not, if not, loop sound.
        ALenum state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            alSourcePlay(source);
            return "Looping " + id.toStdString();
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

    // Play with given fileName.
    if (strCommand == "play")
        return play(id);

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
