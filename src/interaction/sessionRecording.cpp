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

#include <openspace/interaction/sessionRecording.h>
#include <openspace/interaction/externInteraction.h>

#include <openspace/openspace.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/engine/wrapper/windowwrapper.h>
#include <openspace/interaction/navigationhandler.h>
#include <openspace/interaction/orbitalnavigator.h>
#include <openspace/interaction/keyframenavigator.h>
#include <openspace/scene/scenegraphnode.h>
#include <openspace/scripting/scriptscheduler.h>
#include <openspace/util/time.h>
#include <openspace/util/timemanager.h>
#include <openspace/util/spicemanager.h>

#include <ghoul/logging/logmanager.h>

namespace {
    const char* _loggerCat = "SessionRecording";
}

#include "sessionRecording_lua.inl"

namespace openspace::interaction {

SessionRecording::SessionRecording()
    : properties::PropertyOwner({ "SessionRecording" }) {
}

bool SessionRecording::startRecording(std::string filename) {
    if( _state == sessionState::playback ) {
        _playbackFile.close();
    }
    _state = sessionState::recording;
    _playbackActive_camera = false;
    _playbackActive_time = false;
    _playbackActive_script = false;
    _recordFile.open(filename);
    if( ! _recordFile.is_open() || ! _recordFile.good() ) {
        LERROR(fmt::format("Unable to open file {} for keyframe recording", filename.c_str()));
        return false;
    }
    LINFO("Session recording started");
    _timestampRecordStarted = OsEng.windowWrapper().applicationTime();
}

void SessionRecording::stopRecording() {
    if( _state == sessionState::recording )
        _state = sessionState::idle;
    _recordFile.close();
    LINFO("Session recording stopped");
}

bool SessionRecording::startPlayback(std::string filename, KeyframeTimeRef timeMode) {
    _playbackLineNum = 1;
    _playbackFilename = filename;
    _playbackFile.open(_playbackFilename, std::ifstream::in);
    if( ! _playbackFile.is_open() || ! _playbackFile.good() )
    {
        LERROR(fmt::format("Unable to open file {} for keyframe playback",
               filename.c_str()));
        return false;
    }
    LINFO("Playback session started");
    //Set time reference mode
    double now = OsEng.windowWrapper().applicationTime();
    _timestampPlaybackStarted_application = now;
    _timestampPlaybackStarted_simulation = OsEng.timeManager().time().j2000Seconds();
    _timestampApplicationStarted_simulation = _timestampPlaybackStarted_simulation - now;
    _playbackTimeReferenceMode = timeMode;
    //Set playback flags to true for all modes
    _playbackActive_camera = true;
    _playbackActive_time = true;
    _playbackActive_script = true;

    OsEng.navigationHandler().keyframeNavigator().setTimeReferenceMode(timeMode, now);
    OsEng.scriptScheduler().setTimeReferenceMode(timeMode, now);
    playbackAddEntriesToTimeline();

    OsEng.navigationHandler().triggerPlaybackStart([&]() {
        signalPlaybackFinishedForComponent(recordedType::camera);
    });
    OsEng.scriptScheduler().triggerPlaybackStart([&]() {
        signalPlaybackFinishedForComponent(recordedType::script);
    });
    OsEng.timeManager().triggerPlaybackStart([&]() {
        signalPlaybackFinishedForComponent(recordedType::time);
    });
}

void SessionRecording::signalPlaybackFinishedForComponent(recordedType type) {
    if (type == recordedType::camera) {
        _playbackActive_camera = false;
        LINFO("Playback finished signal: camera");
    }
    else if (type == recordedType::time) {
        _playbackActive_time = false;
        LINFO("Playback finished signal: time");
    }
    else if (type == recordedType::script) {
        _playbackActive_script = false;
        LINFO("Playback finished signal: script");
    }

    if (!_playbackActive_camera && !_playbackActive_time && !_playbackActive_script) {
        _playbackFile.close();
        //Reset the script scheduler's time reference to simulation time, which is the
        // default mode for non-playback uses of the scheduler
        OsEng.scriptScheduler().setTimeReferenceMode(
            KeyframeTimeRef::absolute_simTimeJ2000,
            OsEng.windowWrapper().applicationTime());
        LINFO("Playback session finished");
    }
}

void SessionRecording::stopPlayback() {
    OsEng.navigationHandler().setDisableKeyFrameInteraction();
    _playbackFile.close();
    LINFO("Playback session was stopped");
}

void SessionRecording::saveCameraKeyframe() {
    SceneGraphNode* focusNode = OsEng.navigationHandler().focusNode();
    if (!focusNode) {
        return;
    }

    //Create a camera keyframe, then call to populate it with current position
    // & orientation of camera
    datamessagestructures::CameraKeyframe kf;
    _externInteract.generateCameraKeyframe(kf);
    
    std::stringstream keyframeLine = std::stringstream();
    //Add simulation timestamp, timestamp relative, simulation time to recording start
    keyframeLine << "camera ";
    keyframeLine << kf._timestamp << " ";
    keyframeLine << (kf._timestamp - _timestampRecordStarted) << " ";
    keyframeLine << std::fixed << std::setprecision(3) << OsEng.timeManager().time().j2000Seconds();
    keyframeLine << " ";
    //Add camera position
    keyframeLine << kf._position.x << " "
                 << kf._position.y << " "
                 << kf._position.z << " ";
    //Add camera rotation
    keyframeLine << kf._rotation.x << " "
                 << kf._rotation.y << " "
                 << kf._rotation.z << " "
                 << kf._rotation.w << " ";
    if( kf._followNodeRotation )
        keyframeLine << "F ";
    else
        keyframeLine << "- ";
    keyframeLine << kf._focusNode;
    
    saveKeyframeToFile(keyframeLine.str());
}

void SessionRecording::saveTimeKeyframe() {
    //Create a time keyframe, then call to populate it with current time props
    datamessagestructures::TimeKeyframe kf;
    _externInteract.generateTimeKeyframe(kf);

    std::stringstream keyframeLine = std::stringstream();
    //Add simulation timestamp, timestamp relative, simulation time to recording start
    keyframeLine << "time ";
    keyframeLine << kf._timestamp << " ";
    keyframeLine << (kf._timestamp - _timestampRecordStarted) << " ";

    //keyframeLine << std::fixed << std::setprecision(3) << OsEng.timeManager().time().j2000Seconds();
    keyframeLine << std::fixed << std::setprecision(3) << kf._time;

    keyframeLine << " " << kf._dt;
    if( kf._paused )
        keyframeLine << " P";
    else
        keyframeLine << " R";
    
    if( kf._requiresTimeJump )
        keyframeLine << " J";
    else
        keyframeLine << " -";
    
    saveKeyframeToFile(keyframeLine.str());
}

void SessionRecording::saveScript(std::string scriptToSave) {
    datamessagestructures::ScriptMessage sm;
    _externInteract.generateScriptMessage(sm, scriptToSave);

    std::stringstream keyframeLine = std::stringstream();
    //Add simulation timestamp, timestamp relative, simulation time to recording start
    keyframeLine << "script ";
    keyframeLine << sm._timestamp << " ";
    keyframeLine << (sm._timestamp - _timestampRecordStarted) << " ";
    keyframeLine << std::fixed << std::setprecision(3) << OsEng.timeManager().time().j2000Seconds();
    keyframeLine << " ";

    keyframeLine << scriptToSave;
    
    saveKeyframeToFile(keyframeLine.str());
}

void SessionRecording::preSynchronization() {
    if( _state == sessionState::recording ) {
        saveCameraKeyframe();
        saveTimeKeyframe();
    }
}

bool SessionRecording::isRecording() {
    return (_state == sessionState::recording);
}

void SessionRecording::playbackAddEntriesToTimeline() {
    std::string line;
    while( std::getline(_playbackFile, line) ) {
        _playbackLineNum++;
        std::istringstream iss(line);
        std::string entryType;
        if( ! (iss >> entryType) ) {
            LERROR(fmt::format("Error reading entry type @ line {} of playback file {}",
                _playbackLineNum, _playbackFilename.c_str()));
            break;
        }

        if( entryType.compare("camera") == 0 )
            playbackCamera(line);
        else if( entryType.compare("time") == 0 )
            playbackTimeChange(line);
        else if( entryType.compare("script") == 0 )
            playbackScript(line);
    }
    LINFO(fmt::format("Finished parsing {} entries from playback file {}",
        _playbackLineNum - 1, _playbackFilename.c_str()));
}

double SessionRecording::getAppropriateTimestamp(std::istringstream& inputLine) {
    double timeOs, timeRel, timeSim;
    inputLine >> timeOs >> timeRel >> timeSim;

    if (_playbackTimeReferenceMode == KeyframeTimeRef::relative_recordedStart)
        return timeRel;
    else if (_playbackTimeReferenceMode == KeyframeTimeRef::absolute_simTimeJ2000)
        return timeSim;
    else
        return timeOs;
}

double SessionRecording::getEquivalentSimulationTime(std::istringstream& inputLine) {
    double timeOs, timeRel, timeSim;
    inputLine >> timeOs >> timeRel >> timeSim;
    double offset = 0;

    if (_playbackTimeReferenceMode == KeyframeTimeRef::relative_recordedStart)
        offset = timeRel;
    else if (_playbackTimeReferenceMode == KeyframeTimeRef::relative_applicationStart)
        offset = timeOs;

    return _timestampPlaybackStarted_simulation + offset;
}

double SessionRecording::getEquivalentApplicationTime(std::istringstream& inputLine) {
    double timeOs, timeRel, timeSim;
    inputLine >> timeOs >> timeRel >> timeSim;
    double offset = 0;

    if (_playbackTimeReferenceMode == KeyframeTimeRef::relative_recordedStart)
        return _timestampPlaybackStarted_application + timeRel;
    else if (_playbackTimeReferenceMode == KeyframeTimeRef::absolute_simTimeJ2000)
        return timeSim - _timestampApplicationStarted_simulation;
    else
        return timeOs;
}
    
void SessionRecording::playbackCamera(std::string& entry) {
    std::istringstream iss(entry);
    std::string entryType;
    double timeRef;
    std::string rotationFollowing;
    
    interaction::KeyframeNavigator::CameraPose pbFrame;

    iss >> entryType;
    timeRef = getAppropriateTimestamp(iss);
    iss >> pbFrame.position.x
        >> pbFrame.position.y
        >> pbFrame.position.z
        >> pbFrame.rotation.x
        >> pbFrame.rotation.y
        >> pbFrame.rotation.z
        >> pbFrame.rotation.w
        >> rotationFollowing
        >> pbFrame.focusNode;
    if( iss.fail() || !iss.eof() ) {
        LERROR(fmt::format("Error parsing camera line {} of playback file", _playbackLineNum));
        return;
    }

    if( rotationFollowing.compare("F") == 0 )
        pbFrame.followFocusNodeRotation = true;
    else
        pbFrame.followFocusNodeRotation = false;
    
    OsEng.navigationHandler().keyframeNavigator().addKeyframe(timeRef, pbFrame);
}

void SessionRecording::playbackTimeChange(std::string& entry) {
    std::istringstream iss(entry);
    std::string entryType;
    //double timeRef;
    std::string paused, jump;

    datamessagestructures::TimeKeyframe pbFrame;
    iss >> entryType;
    /*timeRef*/ pbFrame._timestamp = getEquivalentApplicationTime(iss);
    iss >> pbFrame._dt
        >> paused
        >> jump;
    if( iss.fail() || !iss.eof() ) {
        LERROR(fmt::format("Error parsing time line {} of playback file", _playbackLineNum));
        return;
    }

    if( paused.compare("P") == 0 )
        pbFrame._paused = true;
    else
        pbFrame._paused = false;
    if( jump.compare("J") == 0 )
        pbFrame._requiresTimeJump = true;
    else
        pbFrame._requiresTimeJump = false;

    pbFrame._time = pbFrame._timestamp + _timestampApplicationStarted_simulation;
    //OsEng.timeManager().addKeyframe(timeRef, pbFrame._timestamp);
    _externInteract.timeInteraction(pbFrame);
}

void SessionRecording::playbackScript(std::string& entry) {
    std::istringstream iss(entry);
    std::string entryType;
    double timeRef;
    
    datamessagestructures::ScriptMessage pbFrame;
    iss >> entryType;
    timeRef = getEquivalentSimulationTime(iss);
    getline(iss, pbFrame._script);
    if( iss.fail() || !iss.eof() ) {
        LERROR(fmt::format("Error parsing script line {} of playback file", _playbackLineNum));
        return;
    }
    //Call script scheduler with this new script entry
    //ghoul::Dictionary scriptDict(std::pair<std::string, std::string>(static_cast<std::string>(KeyForwardScript), pbFrame._script));


    //using namespace std::string_literals;
    //ghoul::Dictionary scriptDict(ghoul::Dictionary{ { KeyTime, "2018 JUL 26 21:15:15.345"s }, {KeyForwardScript, pbFrame._script} } );


    std::string timeDescription = SpiceManager::ref().dateFromEphemerisTime(timeRef, "YYYY MON DD HR:MN:SC.###");

    ghoul::Dictionary scriptDict(ghoul::Dictionary{ {KeyTime, timeDescription },
                                                    {KeyForwardScript, pbFrame._script} }
                                );
    OsEng.scriptScheduler().loadScripts({ { "1", scriptDict } });
}

void SessionRecording::saveKeyframeToFile(std::string entry) {
    _recordFile << entry << std::endl;
}

scripting::LuaLibrary SessionRecording::luaLibrary() {
    return {
        "sessionRecording",
        {
            {
                "startRecording",
                &luascriptfunctions::startRecording,
                {},
                "string",
                "Starts a recording session. The string argument is the filename used "
                "for the file where the recorded keyframes are saved"
            },
            {
                "stopRecording",
                &luascriptfunctions::stopRecording,
                {},
                "void",
                "Stops a recording session"
            },
            {
                "startPlaybackApplicationTime",
                &luascriptfunctions::startPlaybackApplicationTime,
                {},
                "string",
                "Starts a playback session with keyframe times that are relative to "
                "application time (seconds since OpenSpace application started). "
                "The string argument is the filename to pull playback keyframes from."
            },
            {
                "startPlaybackRecordedTime",
                &luascriptfunctions::startPlaybackRecordedTime,
                {},
                "string",
                "Starts a playback session with keyframe times that are relative to "
                "the time since the recording was started (the same relative time "
                "applies to the playback). The string argument is the filename to pull "
                "playback keyframes from."
            },
            {
                "startPlaybackSimulationTime",
                &luascriptfunctions::startPlaybackSimulationTime,
                {},
                "string",
                "Starts a playback session with keyframe times that are relative to "
                "the simulated date & time. The string argument is the filename to pull "
                "playback keyframes from."
            },
            {
                "stopPlayback",
                &luascriptfunctions::stopPlayback,
                {},
                "void",
                "Stops a playback session before playback of all keyframes is complete"
            }
        }
    };
}

} // namespace openspace::interaction
