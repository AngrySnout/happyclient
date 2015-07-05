# [happy] Sauerbraten Client

A custom client for the open-source game [Cube 2: Sauerbraten](http://sauerbraten.org/).

I tried a number of Sauerbraten clients, and found that they are either lacking in features, or are somewhat bloated and confusing. So, I decided to make a new one.
[happy] Sauerbraten Client is meant to be a simple to use, feature rich client, that has everything you'd expect from popular clients and more, while keeping almost everything optional in order to keep the experience as close as possible to the original client.

Currently binaries are only available for the Windows platform, and I haven't tested the client on other platforms. I'd appreciate it if someone is willing to do testing and to contribute binaries for other platforms.

**Latest release:** V0.9.7 (Alpha)


## Features

- **Scoreboard:**
	1. Show current flag holder.
	2. Show flags scored.
	3. Show frags.
	4. Show deaths.
	5. Show accuracy.
	6. Show KPD.
	7. Show teamkills.
	8. Show spectators' pings.
	9. Show country.
	10. Show damage dealt.
	11. Different color for players using auth.
	12. Highlight friends.


- **Server browser:**
	1. Server country in server browser.
	2. Find players.
	3. Filter servers by description (name), master mode, and more.
	4. Preview server before connecting.
	5. Quick connect.


- **HUD:**
	1. Highlight ammo when less than a specific amount.
	2. Display life bar above teammates' heads.
	3. Extra console for frags with an optional kill icon.
	4. Chat word highlighting.
	5. In-game IRC.


- **GUI:**
	1. Show IP in master menu.
	2. New backgrounds.
	3. Global stats tracking.
	4. Screenshots are named using the format "ss_Y_m_d_H_M_S_tm".
	5. ``$DATE`` variable in log file path. Eg start the game with ``-glogs/log_$DATE.txt`` to create a new log each time you run the game.
	6. Auto sorry/np.


- **Weapons:**
	1. Option to make rifle shots look like lightning.
	2. Option to make pistol and chaingun shots blue.


- **Demos**: Client side demo recording is copied from the (ComEd client)[https://github.com/sauerworld/community-edition].
	Commands and variables:
	
	1. ``/cdemoauto [0|1|2]`` 0 = disable, 1 = ask to keep demo everytime, 2 = always keep demo.
	2. ``/cdemostart [name]`` to start recording demo.
	3. ``/cdemostop`` to stop recording demo.
	4. ``/say_nocdemo [text]`` to say something that won't get recorded in the demo.
	5. ``/cdemodir [name]`` to specify the demo directory ("demos" by default).
	6. ``/demo_ [name]`` to play a demo from the demo directory (supports autocompletion).
	7. ``/demotime [minutes_left] [seconds_left]`` and ``/demoskip [minutes] [seconds]`` to traverse a demo (only work forward).


- **CubeScript:**
	1. Quick team chat.
	2. Sorry/np binds (eg ``/bind g [sayteam (concatword "Sorry for the teamkill " $teamkilled "!")]`` and ``/bind v [sayteam (concatword "No problem " $teamkiller "!")]``).


- **Stalker:**
	1. Enabled and disabled from the "new options" menu.
	2. Option to print whois messages for all players upon connection.
	3. ``/whois [cn]`` to query names for a player (can also be accessed from the "master" menu).
	4. ``/whoisname [name] [whole_name_match]`` to find players with a specific name.
	5. ``/whoisip [ip_address]`` to find players with a specific ip (mask).
	6. ``/ipcountry [ip_address]`` to find the country of an ip address.


- **Translation:** You can translate text using the Yandex or MyMemory translation API.
	1. **Yandex:**	In order to use Yandex you have to obtain an API key from [here](https://tech.yandex.com/keys/get/?service=trnsl). The API key might take a few minutes to activate.  
	Once you have your API key use ``/trans_yandex_apikey [api_key]`` in-game. You can now use the translation commands with Yandex.  
	You are allowed to translate 1,000,000 letters per day but no more than 10,000,000 per month. Auto language detection is supported.
	2. **MyMemory:**	When trans_yandex_apikey isn't set MyMemory translation is used. It does not require an API key.  
	You are allowed to translate 1,000 words per day, and if you provide a (valid) email address with ``/trans_mymemory_email [email_address]``, you can translate up to 10,000 words per day.

	You can use these variables and commands to translate:
	
	1. ``/trans_to [language]`` set to "en" by default, the language you want to translate to, as an [ISO 639-1 code](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes).
	2. ``/trans [text]`` auto-detect language of text, translate it, and print the result on the console (works only with Yandex).
	3. ``/trans_ [from_language] [text]`` translate text from from_language (also as an ISO 639-1 code) and print it on the console. Both Yandex and MyMemory are supported.
	4. ``/trans_chat [line_number]`` translate text directly from chat, where line_number is the line you want to translate (1 = last line, 2 = second to last, ...) To bind keypad numbers from 1 to 6 to trans_chat, execute the script "data/trans_binds.cfg" (ie ``/exec data/trans_binds.cfg``).
	5. ``/trans_late [from_language] [to_language] [text] [callback_function]`` translate text from from_language to to_language and call callback_function when the translation is complete. callback_function can use the vars "trans_from" to get the source language (useful if language is auto detected), and "trans_lation" to get the translation of the text.
	6. ``/trans_say [from_language] [to_language] [text]`` translate text from from_language to to_language and say the translation in chat (useful to communicate with foreign players).
	7. ``/trans_sayteam [from_language] [to_language] [text]]`` translate text from from_language to to_language and say the translation in team chat.


## Upcoming

- Search through demos.
- Traverse demos backwards.
- Event system.
- HTTP request support.


## Scraped

- ~~Player name auto-complete.~~
- ~~Cubescript vectors.~~
- ~~Anti-cheat file verification system with closed source executable.~~


## Bugs/problems/suggestions:

You can report anything in the [[happy] clan Sauerbraten client](http://happysauerclan.webs.com/apps/forums/topics/show/12939770-happy-clan-sauerbraten-client) thread.

This client is still in alpha. You will run into bugs. Please report any bugs, problems, or suggestions you have on this thread and I will work on fixing/adding them ASAP.
