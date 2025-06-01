# 3G-usbcdc-esp32-webrtc

**Stream JPEG images from ESP32-S3 (Seeed XIAO Sense) to web browsers via WebRTC using PPP over 2G/3G cellular, utilizing USB CDC for connecting esp32 to modem.**
- Utilizes **ESP32-S3** + **SIM7670** module (USB CDC).
- Achieves **10+ FPS at 640x480 (VGA)** on 3G.
- Does NAT traversal with TURN/STUN
- Implements custom WebRTC signaling server
- Use USB CDC (much faster than UART) to modem for faster data

---

##  Components

###  ESP32 Side (`main/app_main.c`)
- WebRTC: via `libpeer`
- PPP: `esp_modem` + USB CDC
- Camera: captures JPEG images
- Tasks:
  - `camera_task`
  - `peer_connection_task`
  - `peer_signaling_task`

###  Server Side (`webserver-code/WebRTCserver.js`)
- HTTPS server (Express.js with TLS)
- WebSocket signaling
- WHIP endpoint `/whip`
- ICE candidate forwarding

###  Cellular (via `components/ppp-gsm`)
- AT command modem control
- Operator registration
- USB + UART supported

---

##  WebRTC Flow

ESP32 SDP Offer: The ESP32 creates an SDP offer containing media capabilities and sends it via HTTP POST to the /whip endpoint.

Signaling Server Processing: The Express.js server receives the request, processes the SDP offer, and generates an SDP answer.

HTTP Response: Server responds with HTTP 201 status and includes SDP answer plus ICE candidates in the response body.

ESP32 Configuration: The ESP32 sets the received SDP answer as its remote description and begins ICE candidate gathering.

Browser Connection: Browser connects to the signaling server via WebSocket and requests access to an ESP32 stream.

ICE Relay: The server acts as intermediary, relaying ICE candidates between the ESP32 and browser.

P2P Establishment: Once sufficient candidates are exchanged, peers establish a direct connection, potentially via TURN servers if needed.

---

##  Network Configuration

- **TURN**: `turn:standard.relay.metered.ca`
- **STUN**: `stun:stun.l.google.com:19302`
- **Transport**: DTLS-SRTP (secure media)

---

###  Seeed XIAO ESP32S3 USB Pinout
###  SIM7670 / A7670E Module

![Pinouts](https://github.com/gruncode/3G-usbcdc-esp32-webrtc/blob/main/Modules.png?raw=true)

> If using UART, set baud rate:

```txt
AT+IPR=921600
AT+IPREX=921600
AT&W
```

---

## Signaling and Web server Setup (Cloud VM)

```bash
sudo apt install nginx nodejs npm
npm install express cors ws https
```

1. Place certs in `/etc/letsencrypt/live/`
2. Copy server code:
   ```bash
   cp -r webserver-code/* /usr/share/nginx/html/webrtcserver/
   Do a search projectwise and Change CLOUD_SERVER_NAME found anywhere, with your VM name.
   ```
3. Start signaling server:
   ```bash
   cd /usr/share/nginx/html/webrtcserver/
   npx cross-env DEBUG=webrtc,srtp,dtls,running:verbose node WebRTCserver.js
     (This command is very verbose)
   ```
4. Configure and start nginx (`/etc/nginx/sites-available/default`)
5. You also need to change the credentials for TURN servers (I used metered) in main
6. Visit: `http://<your-vm-ip>:8001/webrtcserver/`to view the video

---

##  Full Log Output at esp32

<details>
<summary>📄 Click to expand ESP32 + server logs</summary>

```log
I (1192) main_task: Started on CPU0
I (1212) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1212) main_task: Calling app_main()
I (1212) webrtc: [APP] Startup..
I (1212) webrtc: [APP] Free memory: 8698328 bytes
I (1222) webrtc: [APP] IDF version: v5.4-dirty
I (1222) -------gt_pppos: Initializing esp_modem with USB connection
I (1232) -------gt_pppos: Initializing esp_modem for the SIM7670/A7670 module via USB...
I (1242) -------gt_pppos: Waiting for USB device connection...
I (10272) -------gt_pppos: USB modem connected, waiting 5 seconds for boot...
I (15272) -------gt_pppos: Modem in command mode now.
' (15272) >>>: 'AT+CGDCONT=1,"IP","internet.vodafone.gr"
' (15862) >>>: 'AT+CSTT="internet.vodafone.gr"
' (16062) >>>: 'AT+CLTS=1
' (16262) >>>: 'AT&W
' (16642) >>>: 'AT+COPS?
I (16642) -------gt_pppos: AT+COPS? => response="+COPS: 0,2,"20205",7", err=ESP_OK 
' (16642) >>>: 'AT+CSQ
I (16642) -------gt_pppos: Operator found quickly with no reset needed
' (16642) >>>: 'AT
' (16652) >>>: 'AT+CIPSHUT
' (16852) >>>: 'AT+CGACT=0
' (17052) >>>: 'AT+SAPBR=0,1
I (17252) -------gt_pppos: Calling set_data_mode (attempt 1/2)
' (17252) >>>: 'ATE0
' (17252) >>>: 'AT+CGDCONT=1,"IP","internet.vodafone.gr"
' (17252) >>>: 'ATD*99#
I (17342) -------gt_pppos: in set_data_mode : esp_modem_set_mode set ESP_MODEM_MODE_DATA success
I (17352) -------gt_pppos: ~~~~~~~~~~~~~~
I (17352) -------gt_pppos: IP          : 10.202.58.37
I (17352) -------gt_pppos: Netmask     : 255.255.255.255
I (17362) -------gt_pppos: Gateway     : 10.64.64.64
I (17362) esp-netif_lwip-ppp: Connected
I (17362) -------gt_pppos: Name Server1: 62.74.130.15
I (17372) -------gt_pppos: Name Server2: 62.74.131.12
I (17422) s3 ll_cam: DMA Channel=0
I (17422) cam_hal: cam init ok
I (17422) sccb: pin_sda 40 pin_scl 39
I (17422) sccb: sccb_i2c_port=1
I (17432) camera: Detected camera at address=0x30
I (17432) camera: Detected OV2640 camera
I (17432) camera: Camera PID=0x26 VER=0x42 MIDL=0x7f MIDH=0xa2
I (17502) cam_hal: buffer_size: 16384, half_buffer_size: 1024, node_buffer_size: 1024, node_cnt: 16, total_cnt: 60
I (17502) cam_hal: Allocating 61440 Byte frame buffer in PSRAM
I (17512) cam_hal: Allocating 61440 Byte frame buffer in PSRAM
I (17512) cam_hal: Allocating 61440 Byte frame buffer in PSRAM
I (17522) cam_hal: Allocating 61440 Byte frame buffer in PSRAM
I (17522) cam_hal: cam config ok
I (17532) ov2640: Set PLL: clk_2x: 0, clk_div: 0, pclk_auto: 0, pclk_div: 8
I (17592) ov2640: Set PLL: clk_2x: 0, clk_div: 0, pclk_auto: 0, pclk_div: 8
W (17602) cam_hal: NO-SOI
I (17632) Camera: Camera sensor optimized for high FPS cellular transmission
INFO ../components/sepfy__libpeer/src/peer_connection.c 190 Datachannel allocates heap size: 153600
INFO ../components/sepfy__libpeer/src/peer_signaling.c 559 HTTP Host: CLOUD_SERVER_NAME, Port: 8080, Path: /whip
WARN ../components/sepfy__libpeer/src/peer_signaling.c 500 Invalid MQTT port number: -1
I (19102) webrtc: PeerConnectionState: 1
WARN ../components/sepfy__libpeer/src/peer_signaling.c 500 Invalid MQTT port number: -1
I (19112) Camera: Cellular-Optimized Camera Task Started
I (19112) webrtc: [APP] Free memory: 8096652 bytes
I (19122) webrtc: peer_signaling_task started
I (19122) webrtc: peer_connection_task started
INFO ../components/sepfy__libpeer/src/peer_connection.c 289 ice_servers: turns:standard.relay.metered.ca:443?transport=tcp
INFO ../components/sepfy__libpeer/src/agent.c 29 create IPv4 UDP socket: 54
INFO ../components/sepfy__libpeer/src/peer_connection.c 289 ice_servers: turns:standard.relay.metered.ca:443
INFO ../components/sepfy__libpeer/src/agent.c 29 create IPv4 UDP socket: 55
INFO ../components/sepfy__libpeer/src/peer_connection.c 289 ice_servers: stun:stun.l.google.com:19302
INFO ../components/sepfy__libpeer/src/agent.c 29 create IPv4 UDP socket: 56
INFO ../components/sepfy__libpeer/src/ports.c 164 Resolved stun.l.google.com -> 74.125.250.129
INFO ../components/sepfy__libpeer/src/agent.c 273 stun/turn server 74.125.250.129:19302
INFO ../components/sepfy__libpeer/src/peer_signaling.c 268 Connecting to CLOUD_SERVER_NAME:8080/whip
INFO ../components/sepfy__libpeer/src/ports.c 164 Resolved CLOUD_SERVER_NAME -> 158.180.236.80
INFO ../components/sepfy__libpeer/src/socket.c 256 Connecting to server: 158.180.236.80:8080
INFO ../components/sepfy__libpeer/src/socket.c 262 Server is connected
INFO ../components/sepfy__libpeer/src/ssl_transport.c 85 start to handshake
INFO ../components/sepfy__libpeer/src/ssl_transport.c 93 handshake success
INFO ../components/sepfy__libpeer/src/peer_signaling.c 276 Connected successfully, sending HTTP POST request with body length: 529
INFO ../components/sepfy__libpeer/src/peer_signaling.c 288 HTTP request sent, checking response...
INFO ../components/sepfy__libpeer/src/peer_signaling.c 289 Response status: 201
INFO ../components/sepfy__libpeer/src/peer_signaling.c 291 Received HTTP response from CLOUD_SERVER_NAME/whip
I (30842) webrtc: PeerConnectionState: 2
I (36972) webrtc: PeerConnectionState: 3
INFO ../components/sepfy__libpeer/src/dtls_srtp.c 310 Created inbound SRTP session
INFO ../components/sepfy__libpeer/src/dtls_srtp.c 331 Created outbound SRTP session
INFO ../components/sepfy__libpeer/src/peer_connection.c 387 SCTP create socket
I (37642) webrtc: PeerConnectionState: 4
I (38892) webrtc: Datachannel opened
```

</details>

---

## Hardware Support

-  Seeed XIAO ESP32S3 Sense
-  ESP32-EYE (not tested)
   M5Camera (not tested)
