/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2018                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#ifndef __OPENSPACE_CORE___SESSIONRECORDING___H__
#define __OPENSPACE_CORE___SESSIONRECORDING___H__

#include <openspace/interaction/externInteraction.h>
#include <openspace/network/messagestructures.h>

#include <ghoul/io/socket/tcpsocket.h>

#include <vector>
#include <fstream>

namespace openspace {
    
class SessionRecording  {
public:
    enum class sessionState {
        idle = 0,
        recording,
        playback
    };
    SessionRecording();
    bool startRecording(std::string filename);
    void stopRecording();
    void recordCamera();
    void recordTimeChange();
    void recordScript();
    void saveScript(std::string scriptToSave);
    bool isRecording();
    bool startPlayback(std::string filename);
    void saveCameraKeyframe();
    void saveTimeKeyframe();
    void preSynchronization();
    void playbackAddEntriesToTimeline();
    void playbackCamera(std::string& entry);
    void playbackTimeChange(std::string& entry);
    void playbackScript(std::string& entry);
    
private:
    ExternInteraction _externInteract;
    bool _isRecording = false;
    double _timestampRecordStarted;
    void saveKeyframeToFile(std::string entry);
    sessionState _state = sessionState::idle;
    std::string _playbackFilename;
    std::ifstream _playbackFile;
    std::ofstream _recordFile;
    int _playbackLineNum = 1;
};

} // namespace openspace

#endif // __OPENSPACE_CORE___SESSIONRECORDING___H__