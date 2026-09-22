# JUP_Custom_Camo_PoC

[ About ]<br><br>

This code has been verified to work in CoD: MWIII Season 6 "The Haunting."<br>
Please prepare the client code yourself and adjust it to suit your own code.<br><br>

-----

[ Technical Limitations ]<br><br>

In MWIII, GfxImage data for camo images is loaded from the fastfile when the Load_GfxImage function runs, then taken into the Asset pool area by DB_AddXAssets.<br>
At that time, GfxImages with the Stream flag set are pulled into the stream area.<br>
On the old IW8 engine, even images that had entered the Stream area could be changed at runtime by running Image_SetupInternal, but on the JUP engine, rewriting a Stream-requested GfxImage afterward causes a crash.<br>
As a bypass for this limitation, the Stream flag is cleared at the timing when Load_GfxImage runs during the splash screen at game launch.<br>
After that, Image_SetupInternal is run so that GfxImages unpacked from the fastfile are treated as non-Stream, and the images can still be swapped later at runtime.<br>
However, the timing at which the JUP engine updates GfxImages changes between frontend and in-game loading, so even if you change the image, the appearance will not update unless you go back and forth between the frontend and in-game.<br>
I hope someone finds a way to change it at runtime in the future.<br>
Also, I haven't tried this yet, but if you keep swapping images, there is a possibility of crashing due to exceeding a memory leak.<br>
This release is intended as a technical PoC for custom camos, so please understand that memory leak investigation and countermeasures have not been done.<br>

-----

[ How to use the source code ]<br><br>

I added the `#include` statements somewhat arbitrarily, so please remove any that aren't needed.<br>
Functions like `NotifyMsg` are specific to my client, so please replace them with `printf` or similar.<br><br>

Only three engine functions are primarily required for this custom camo implementation:<br>
_adr.DB_FindXAssetHeader	= 0x3BD5900;<br>
_adr.Image_SetupInternal	= 0x55D0560;<br>
_adr.Load_GfxImage			= 0x3B0B730;<br><br>

After hooking `Load_GfxImage`, the hashed GfxImage asset name is automatically passed to the function.<br>
If a user-defined hash table with a matching entry exists in the JSON file, the system loads the image from the specified relative file path and swaps it in.<br>
Injecting the image clears the "Stream" flag, allowing you to change the camo image again at any time.<br><br>

To overwrite the camo again, execute a method like `TryLoadJupCustomGfxImage( "camo_a_01" );` using the non-hashed camo name.<br>
This traces back from the Camo structure to access the `textureGroup0` field (the main texture).<br>
From there, the standard custom camo processing takes over.<br><br>

As noted above, repeatedly overwriting the image may cause memory leaks.<br>
Ideally, the best approach is to add a custom GfxImage using `DB_AddXAssets` and swap that in instead.<br>
However, the JUP engine uses hashed asset names, making name resolution cumbersome, so I haven't investigated that method for this implementation.<br>
Feel free to use and improve the source code as you see fit.<br>

-----

# [ Self-Introduction ]

Thank you for watching.<br>
I would be very happy if you would follow, subscribe to, bookmark, like, and comment on my various social media accounts.<br>

- X (Twitter) : https://x.com/hinatyu
- YouTube : https://www.youtube.com/@HiNAtyuRoom
- TikTok : https://www.tiktok.com/@hinatyustudio
- GitHub : https://github.com/ProjectHiNAtyu

-----

# 【 Promotion 】

If you like my work, I'd be happy if you could sponsor or donate.<br>
This will help facilitate the development of new features and fixes.<br>

Please note that this is literally a "donation"; it is not a mandatory payment or a demand for money.<br>
As it is a donation, I cannot guarantee any specific return or benefit in exchange for the contribution.<br>
Please understand that this is similar in nature to the "Super Chat" feature often seen on YouTube.<br>
Development will continue regardless of whether donations are received, so please contribute only if you wish to do so.<br>
That said, I do want to mention that your kind generosity boosts my motivation and could potentially lead to the development of new features.<br>

- Ko-fi: ttps://ko-fi.com/hinatyustudio
- BTC : 32J66dfWi9dqqWHS2RYR9rFCUNBL88vgUR
- ETH: 0xaE5D5b3e8E865B2bA676a24eF41d5f4CBD315978

-----