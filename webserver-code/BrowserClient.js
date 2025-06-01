// Wait for DOM to be fully loaded before initializing WebSocket
document.addEventListener('DOMContentLoaded', function() {
  initializeWebRTC();
});

const signalingServerUrl = 'wss://CLOUD_SERVER_NAME:8080?clientId=browserClient';

// Global variables
let videoElement, audioElement;
let isPlaying = true;
let isMuted = false;
let hasVolume = false;
let rotate = 0;

// FPS update logic
let frameCount = 0;
let fpsStartTime = performance.now();

function updateFPS() {
  frameCount++;
  const currentTime = performance.now();
  const elapsedTime = currentTime - fpsStartTime;
  
  // Update FPS every second
  if (elapsedTime >= 1000) {
    const fps = (frameCount * 1000) / elapsedTime;
    const fpsDisplay = document.getElementById('fps-display');
    if (fpsDisplay) {
      fpsDisplay.textContent = `FPS: ${fps.toFixed(2)}`;
    }
    
    // Reset counters
    frameCount = 0;
    fpsStartTime = currentTime;
  }
}

function initializeWebRTC() {
  // Get DOM elements after page loads
  videoElement = document.getElementById('imgStream');
  audioElement = document.getElementById('audioStream');
  
  if (!videoElement || !audioElement) {
    console.error('Required DOM elements not found');
    return;
  }

  // WebRTC peer connection
  const pc = new RTCPeerConnection({
    iceServers: [
      {
        urls: "turns:standard.relay.metered.ca:443?transport=tcp",
        username: "**********Your Username********",
        credential: "**********Your credentials********",
      },
      {
        urls: "turn:standard.relay.metered.ca:443",
        username: "**********Your Username********",
        credential: "**********Your credentials********",
      },
      {
        urls: "stun:stun.l.google.com:19302",
      },
    ],
  });

  const dataChannel = pc.createDataChannel('imageChannel');
  const ws = new WebSocket(signalingServerUrl);

  // WebSocket message handling
  ws.onmessage = async (event) => {
    const message = JSON.parse(event.data);

    if (message.type === 'sdp') {
      const sdp = message.sdp?.sdp || message.sdp;
      const sdpType = message.sdp?.type || 'offer';
      const modifiedSDP = modifySDPForPCM(sdp);
      const remoteDescription = new RTCSessionDescription({ type: sdpType, sdp: modifiedSDP });

      try {
        await pc.setRemoteDescription(remoteDescription);
        if (remoteDescription.type === 'offer') {
          const sdpAnswer = await pc.createAnswer();
          await pc.setLocalDescription(sdpAnswer);
          ws.send(JSON.stringify({ type: 'sdp', sdp: sdpAnswer }));
        }
      } catch (error) {
        console.error('Error setting remote SDP:', error);
      }
    } else if (message.type === 'candidate') {
      try {
        await pc.addIceCandidate(message.candidate);
      } catch (error) {
        console.error('Error adding ICE candidate:', error);
      }
    }
  };

  // ICE candidates
  pc.onicecandidate = (event) => {
    if (event.candidate) {
      ws.send(JSON.stringify({ type: 'candidate', candidate: event.candidate }));
    }
  };

  // Audio track handler
  pc.ontrack = function (event) {
    if (event.track.kind === 'audio') {
      const audioContext = new AudioContext();
      const stream = new MediaStream();
      stream.addTrack(event.track);
      const source = audioContext.createMediaStreamSource(stream);
      source.connect(audioContext.destination);

      if (audioElement) {
        audioElement.srcObject = stream;
        audioElement.controls = true;
        audioElement.muted = false;
        audioElement.play().catch(console.error);
      }
    }
  };

  // Modify SDP to support PCMA audio
  function modifySDPForPCM(sdp) {
    const sdpLines = sdp.split('\r\n');
    for (let i = 0; i < sdpLines.length; i++) {
      if (sdpLines[i].startsWith('m=audio')) {
        sdpLines[i] = sdpLines[i].replace('RTP/SAVPF', 'RTP/SAVP') + ' 8';
        if (!sdpLines.includes('a=rtpmap:8 PCMA/8000')) {
          sdpLines.splice(i + 1, 0, 'a=rtpmap:8 PCMA/8000');
        }
      }
    }
    return sdpLines.join('\r\n');
  }

  // Data channel handlers
  dataChannel.onopen = () => console.log('Data channel opened');
  dataChannel.onclose = () => console.log('Data channel closed');

  dataChannel.onmessage = (event) => {
    if (event.data instanceof ArrayBuffer) {
      const blob = new Blob([event.data], { type: 'image/jpeg' });
      const imageUrl = URL.createObjectURL(blob);
      videoElement.src = imageUrl;

      // Call updateFPS when image loads to count frames
      videoElement.onload = () => {
        URL.revokeObjectURL(imageUrl);  // Clean up the blob
        updateFPS();                    // Count this frame for FPS calculation
      };
    }
  };

  // WebSocket logging
  ws.onopen = () => console.log('WebSocket connected');
  ws.onerror = (error) => console.error('WebSocket error:', error);
  ws.onclose = (event) => console.log('WebSocket closed', event.code, event.reason);
}

// UI button functions
function onRotate() {
  rotate = (rotate + 180) % 360;
  videoElement.style.transform = `rotate(${rotate}deg)`;
}

function onVolume() {
  hasVolume = !hasVolume;
  if (hasVolume) {
    audioElement.play();
    document.getElementById('volume-icon').className = 'fa-solid fa-volume-high';
  } else {
    audioElement.pause();
    document.getElementById('volume-icon').className = 'fa-solid fa-volume-xmark';
  }
}

function onMuted() {
  isMuted = !isMuted;
  audioElement.muted = isMuted;
  document.getElementById('mute-icon').className = isMuted ? 'fa-solid fa-microphone-slash' : 'fa-solid fa-microphone';
}

function onStop() {
  isPlaying = !isPlaying;
  if (isPlaying) {
    videoElement.play();
    document.getElementById('stop-icon').className = 'fa-solid fa-circle-stop';
  } else {
    videoElement.pause();
    document.getElementById('stop-icon').className = 'fa-solid fa-circle-play';
  }
}

// Remove the setInterval - FPS updates automatically when frames arrive
