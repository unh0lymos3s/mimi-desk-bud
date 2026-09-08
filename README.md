# MIMI - DESK BUDDY
MIMI is a diy desk buddy with an insane amount of customization, that you can manage with the user friendly web interface. there are a couple of built in generic animations I found on github but other than those, you can pretty much put any gif/image on the screen, by either 
- Drawing on the canvas
- Uploading Images/gifs from your device
- Choosing a gif from GIPHY search Engine
You can also control your spotify music (Play/Pause) by connecting your spotify account to it.


# HOW TO BUILD

Building MIMI is very simple, let's start with the indgredients, the ones labelled optional wont affect the pre built animations but I highly recommend setting them up to get the most out of MIMI:

1. ESP32-C3 (This will be our motherboard aka MIMI's brain)
2. 1.3" OLED display (This will be da screen)
3. A touch sensor (I don't know the model they all look the same)
4. **OPTIONAL** A Netlify account, free tier works fine (This will be where we host the web interface)
5. **OPTIONAL** A Spotify account (To control and display the current playing song)
4. ARDUINO IDE (To flash the firmware on to the ESP32-C3)

## STEP 1: Making connections

- ESP32-c3 GPIO 8 -> OLED SDA
- ESP32-C3 GPIO 9 -> OLED SCL
- ESP32-C3 G -> OLED GND | Touch Sensor GND (THIS IS A PARALLEL CONNECTION)
- ESP32-C3 3.3/3v3 -> OLED VCC | Touch Sensor VCC (THIS IS ALSO A PARALLEL CONNECTION)

Plug in the ESP32-C3 via a Type-C cable and you will find a red light on the board, and a green light should light up on your touch sensor as well, OLED does not have a light up indicator so don't be scared.

If at any point after plugging the power in should any component start to heat up, unplug immediately and recheck your wiring.

If you want to make it portable connect a battery, charging module and a switch accordingly to the 3v3 on your ESP32-C3.


# STEP 2: Getting Ready

The hard part is over, now:

1. Open the Arduino IDE on your PC
2. Navigate to Tools -> Manage Libraries
3. Now you need to search and install for these libraries: **ArduinoJson** by Benoit, **AdaFruit GFX Library** by AdaFruit (with dependencies)
4. Now navigate to  Tools -> Manage Boards, and install the ESP32 Boards package.
5. Finally on the top from the select board menu, select ESP32C3 dev board, along with the serial port on your device where the ESP32-C3 is connected


# STEP 3: Getting the web server up and running

#  SPOTIFY AUTH FLOW (DO NOT BE SCARED)

1. Create **SPOTIFY DEVELOPER ACCOUNT**
2. Create an application, name it whatever you want, choose the **Spotify WEB API and 127.0.0.1:8888 for callback**  
3. Get your ***Client ID and CLient Secret*** 
4. run this link in your browser (modify the client ID):
          https://accounts.spotify.com/authorize?client_id=YOUR_CLIENT_ID8&response_type=code&redirect_uri=http%3A%2F%2F127.0.0.1%3A8888%2Fcallback&scope=user-read-currently-playing%20user-read-playback-state%20user-modify-playback-state
5. once you authorize, you will be redirected back to 127.0.0.1:8888/callback?code="AUTHORIZATION_CODE"
6. From the above mentioned URL, extract the value of your AUTHORIZATION_CODE
7. In your terminal, run the following command:
    curl -s -X POST https://accounts.spotify.com/api/token   -H "Content-Type: application/x-www-form-urlencoded"   --data-urlencode grant_type=authorization_code   --data-urlencode "code=YOUR_AUTHORIZATION_CODE"   --data-urlencode redirect_uri=http://127.0.0.1:8888/callback   --data-urlencode client_id=YOUR_CLIENT_ID   --data-urlencode client_secret=YOUR_CLIENT_SECRET

8. You will receive an refresh-token in the response packet (it will be structured like this):
      {
        "access_token":"YOUR_ACCESS_TOKEN",
        "token_type":"",
        "expires_in":,
        "refresh_token":"YOUR REFRESH_TOKEN",
        "scope":"user-modify-playback-state user-read-playback-state user-read-currently-playing"
      }
9. Finally, add your CLIENT_ID, CLIENT_SECRET and REFRESH_TOKEN in the respective spots in secrets.h
