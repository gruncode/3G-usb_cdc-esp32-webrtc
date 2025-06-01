const https = require('https');
const fs = require('fs');
const express = require('express');
const cors = require('cors');
const WebSocket = require('ws');
const logger = require('./logger2');

const app = express();
const PORT = 8080;
const BROWSER_ID = 'browserClient';
const loggedCandidates = new Set();
let clients = {};

const options = {
  key: fs.readFileSync('/etc/letsencrypt/live/CLOUD_SERVER_NAME/privkey.pem'),
  cert: fs.readFileSync('/etc/letsencrypt/live/CLOUD_SERVER_NAME/fullchain.pem')
};

// Use CORS middleware
app.use(cors());

// Middleware to parse JSON bodies
app.use(express.json());

// Create HTTPS server
const server = https.createServer(options, app);

// Create WebSocket server
const wss = new WebSocket.Server({ server });

server.listen(PORT, '0.0.0.0', () => {
  logger.info(`-----------HTTPS server is listening on port ${PORT} for IPv4`);
});

server.listen(PORT, '::', () => {
  logger.debug(`-----------HTTPS server is listening on port ${PORT} for IPv6`);
});

app.post('/whip', (req, res) => {
  logger.info('-----------Received request to /whip endpoint');

  let body = '';
  req.on('data', chunk => {
    body += chunk.toString();
    logger.info('-----------Received body chunk');
  });

  req.on('end', async () => {
    logger.info('-----------req.on(end Finished receiving data\n');
    try {
        let sdp = null;
      if (req.headers['content-type'] === 'application/sdp') {
        sdp = { type: 'offer', sdp: body };
        logger.info('-----------Received plain SDP:\n', sdp);
      } else {
        logger.info('-----------Received NOT plain SDP\n');
      }

      if (typeof sdp !== 'string' && sdp.sdp) {
        sdp = sdp.sdp;
      }

      logger.info('-----------After sdp !== string && sdp.sdp:\n', sdp);
      logger.info('-----------After sdp.sdp.replace cr :\n' + sdp.replace(/\r\n/g, '\n'));

      if (!clients["ESP"]) {
        
        clients["ESP"] = { sdp: sdp, response: res };
        // clients["ESP"].sdp = sdp;
        // clients["ESP"].response = res; 

        logger.info(`-----------Created new client record for ESP\n`);

      }


      // If the browser has already connected, send the offer to the browser
      if (clients[BROWSER_ID] && clients[BROWSER_ID].ws) {

        try {

          clients[BROWSER_ID].ws.send(JSON.stringify({ type: 'sdp', sdp: clients["ESP"].sdp }));

          logger.info('-----------SDP  sent to the browser.');
        } catch (error) {
          console.error(`-----------Error sending SDP to browser `, error);
        }
      }
      else{
        logger.info(`-----------No WebSocket found for BROWSER. Cannot send SDP.`);
      }

    } catch (error) {
      handleError(res, error);
    }

  });


  // Handle connection closure to reset state when ESP connection closes
  res.on('close', () => {
    logger.info('-----------ESP connection closed, resetting state for ESP');
    if (clients["ESP"]) {
      delete clients["ESP"];  // Reset client state
    }
    loggedCandidates.clear(); // Clear any stored ICE candidates
  });

  // Optional: Handle 'finish' event when the response is completed
  res.on('finish', () => {
    logger.info('-----------ESP response finished');
  });
  
});


// Block undefined routes
app.get('*', (req, res) => {
  res.writeHead(404, { 'Content-Type': 'text/plain' });
  res.end('Not Found');
});

wss.on('connection', (ws, req) => {

  let clientId;

  try {
    clientId = new URL(req.url, `https://${req.headers.host}`).searchParams.get('clientId') || 'unknown_client';
  } catch (error) {
    console.error('connection Failed to parse clientId from URL:', error);
    clientId = 'unknown_client';
  }

  logger.warn(`-----------WebSocket established for clientId: ${clientId}`);

  if (clients[clientId]) {
    logger.warn(`-----------Reconnection detected for: ${clientId}. Cleaning up old connection.`);
    if (clients[clientId].ws) {
      clients[clientId].ws.close();
    }
  }

  clients[clientId] = {
    ws: ws,
    sdp: clients[clientId] ? clients[clientId].sdp : null,
    candidates: clients[clientId] ? clients[clientId].candidates : [],
    response: clients[clientId]?.response || null,
    resolve: clients[clientId]?.resolve || null,
  };

  if (clients["ESP"] && clients["ESP"].sdp){

    if (clients[BROWSER_ID] && clients[BROWSER_ID].ws) {
      logger.info(`-----------Forwarding Stored ESP sdp to BROWSER\n`);
      try {
        clients[BROWSER_ID].ws.send(JSON.stringify({ type: 'sdp', sdp: clients["ESP"].sdp }));

        logger.info('-----------send Stored ESP sdp to BROWSER.\n');
      } catch (error) {
        console.error(`----------- Error sending Stored ESP sdp to browser\n`, error);
      }
    } else {
      logger.info(`-----------No WebSocket found for BROWSER. Cannot send Stored ESP sdp.\n`);
    }

  }

  ws.on('message', async (message) => {

    const messageStr = Buffer.isBuffer(message) ? message.toString('utf8') : message;
  
    logger.warn(`@@@@@@@@@@@@@@@@@@@@@@@@@@@@@WS MESSAGE RECEIVED WEBSOCKET messageStr FROM : ${clientId} \n ${messageStr} \n @@@@@@@@@@@@@@@@@@@@@@@@@@@@@`);

    jsonMessage = JSON.parse(messageStr);

    // Log the entire SDP for debugging
    logger.warn('-----------WebSocket JSON.parse(messageStr) SDP:\n', jsonMessage);

    if (jsonMessage) {

      if (jsonMessage.type === 'sdp') {

        const sdp = jsonMessage.sdp.sdp || jsonMessage.sdp;  // Extract the SDP (either string or object)

        clients[clientId].sdp = typeof sdp === 'string' ? sdp : sdp.sdp;  // Ensure SDP is stored as a string

        logger.warn(`-----------Stored SDP in data of : ${clientId}`);
        forwardSdpToEsp(clients[clientId].sdp);     // Send SDP to ESP32

      }
      else if (jsonMessage.type === 'candidate') {


        const processedCandidate = preprocessBrowserCandidate(jsonMessage.candidate);

        storeForwardCandidates(clientId, [processedCandidate]);
      }
    }
  });


  ws.on('error', (err) => handleWebSocketError(clientId, err));

  ws.on('close', () => handleWebSocketClose(clientId));
});





function storeForwardCandidates(clientId, candidates) {

  logger.warn('-----------storeForwardCandidates for :', clientId);
  logger.warn('-----------storeForwardCandidates:', candidates);

  candidates.forEach(parsedCandidate => {

    logger.warn('-----------Parsed ICE Candidate object:', parsedCandidate);

    // Convert the parsed candidate to the format expected by the ESP32
    const candidateString = `a=candidate:${parsedCandidate.foundation} ${parsedCandidate.component} ${parsedCandidate.protocol} `+
                                           `${parsedCandidate.priority} ${parsedCandidate.ip} ${parsedCandidate.port} typ ${parsedCandidate.type}`;

    logger.warn('-----------Generated SDP-formatted candidate:', candidateString);

    const candidateId = `${parsedCandidate.foundation}-${parsedCandidate.component}-${parsedCandidate.protocol}-${parsedCandidate.ip}-${parsedCandidate.port}`;

    logger.warn('-----------Processing candidate for :', clientId);
    logger.warn('-----------Generated Candidate ID:', candidateId);
    logger.warn('-----------loggedCandidates:', loggedCandidates);

    //loggedCandidates store ids so candidates are not duplicated
    if (!loggedCandidates.has(candidateId)) {

      loggedCandidates.add(candidateId);

      if (!clients[clientId].candidates) {

        clients[clientId].candidates = [];
      }

      clients[clientId].candidates.push(candidateString); // Store as a string in SDP format

      logger.warn(`-----------Stored ICE candidate in data of: ${clientId}`);
      logger.warn('-----------Stored ICE Candidate:', candidateString);

      // Log the full clients object, filtered for readability
      logger.warn('-----------Full Clients Object (Filtered):', JSON.stringify(clients, replacer, 2));

      sendCandidateToEsp(candidateString); // Send formatted candidate string

    } else {
      logger.warn('-----------Duplicate Candidate ignored:', candidateId);
    }
  });
}


function sendCandidateToEsp(candidateString) {

  logger.warn('-----------Attempting to send ICE candidate to ESP32');

  // Log the full clients object, filtered for readability
  logger.warn('-----------sendCandidateToEsp Full Clients Object:', JSON.stringify(clients, replacer, 2));

  // Check if the ESP32 client has a stored response object
  if (clients["ESP"] && clients["ESP"].response) {

    logger.warn('-----------candidateString to send to esp:', candidateString);

    try {
      clients["ESP"].response.statusCode = 201;
      clients["ESP"].response.write(candidateString + '\r\n'); // Ensure each candidate is followed by \r\n
      logger.warn('-----------ICE candidate successfully sent to ESP32 via response.write()');
    } catch (error) {
      logger.warn('-----------Error writing ICE candidate to ESP32:', error);
    }
  }
  else {
    logger.warn('No response object found for ESP32');
  }
}

function forwardSdpToEsp(sdpString) {

  logger.warn('-----------Attempting to forward SDP to ESP32');

  // Check if the ESP32 client has a stored response object
  if (clients["ESP"] && clients["ESP"].response) {
    
    try {

      // Ensure SDP is a string before sending it
      logger.warn(`-----------forwardSdpToEsp sdpString=`, sdpString);

      // Write the SDP data to the ESP32 response
      clients["ESP"].response.statusCode = 201;
      clients["ESP"].response.write(sdpString);
      logger.warn('-----------SDP successfully sent to ESP32 via response.write()');
      
    } catch (error) {
      logger.warn('-----------Error writing SDP to ESP32:', error);
    }
  } else {
    logger.warn('No response object found for ESP32');
  }
}



function handleError(res, error) {
    console.error('Error processing /whip request:', error);
    if (!res.headersSent) {
        res.writeHead(400, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Invalid JSON or unsupported data format' }));
    }
}



function handleWebSocketError(clientId, err) {
  console.error(`WebSocket error for ${clientId}`, err);
}

function handleWebSocketClose(clientId) {
  logger.info(`-----------WebSocket connection closed for ${clientId}`);
  delete clients[clientId];
}


function parseCandidate(candidateString) {
  if (!candidateString) {
    console.error('Invalid candidate string:', candidateString);
    return null;
  }

  const parts = candidateString.split(' ');
  if (parts.length < 8) {
    console.error('Candidate string has insufficient parts:', candidateString);
    return null;
  }

  return {
    foundation: parts[0].split(':')[1],
    component: parts[1],
    protocol: parts[2],
    priority: parts[3],
    ip: parts[4],
    port: parts[5],
    type: parts[7],
    relatedAddress: parts[9] || null,
    relatedPort: parts[11] || null,
  };
}



function preprocessBrowserCandidate(candidateObj) {
  const candidateString = candidateObj.candidate;

  // If the candidate string is empty or null, return early
  if (!candidateString) {
    console.error('Invalid candidate string:', candidateString);
    return null;
  }

  const parsedCandidate = parseCandidate(candidateString);

  // If parsing failed, return null
  if (!parsedCandidate) {
    console.error('Failed to parse candidate:', candidateString);
    return null;
  }

  return {
    foundation: parsedCandidate.foundation,
    component: parsedCandidate.component,
    protocol: parsedCandidate.protocol,
    priority: parsedCandidate.priority,
    ip: parsedCandidate.ip,
    port: parsedCandidate.port,
    type: parsedCandidate.type,
  };
}


function replacer(key, value) {
  // Filter out circular references or unnecessary properties
  if (key === 'socket' || key === 'response' || key === 'ws') {
      return '[Filtered]';
  }
  return value;
}

