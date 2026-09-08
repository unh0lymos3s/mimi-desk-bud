#SPOTIFY AUTH FLOW (DO NOT BE SCARED)

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
