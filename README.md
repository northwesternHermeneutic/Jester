# Jester

> **Jester is a personal fork of [CrossInk](https://github.com/uxjulia/CrossInk)** (itself a fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)) for the original **Xteink X4**. It is based on CrossInk v1.6.0, takes selected changes from CrossInk v1.6.1, and keeps the v1.6.0 menus and navigation.

Current version: **1.0.2** (shown on the device as Jester_v1.0.2).

### Supported device

- Xteink X4 (original, ESP32-C3), PlatformIO environment `default`.

Code for the other CrossInk devices (X3, X4 Pro, X4 Classic, Sticky) is still in the tree but is not built, tested or supported by Jester.

## Why Jester

Jester is a reading-first firmware: fewer features, but the ones that matter for reading books (and keeping track of passages in them) work well on a 380 KB device. Improved embedded page number support, they are now an option to add in the status bar for books with them. Reverts the CrossInk v1.6.1 in-book menu and button behaviour to v1.6.0.

---

## Features

### Introducing: Dog Ears! 
#### A quicker, more natural way to save the best bits for later. 

Has this ever happened to you? 

You come across a riveting passage in a book, one that you could read over and over, possibly life changing, that shows the kind of mastery of language that you aspire to. You find yourself faced with three options: 
Bookmark, Clipping, or Screenshot. 

Bookmark? Sounds nice, but fleeting, temporary, stuck to the device without any obvious way to export it. Clipping? Better, now you've at least got the text into a file you can
easily get onto your phone or computer, or just go back to on your X4 without losing your place in the book. But man is it tedious to make, having to add line by line, covering things 
in highlights, never sure how much you can or can't get in a single clipping. How about a Screenshot? Well, that's quick at least, but now what are you supposed to do with all these .bmp
files? Transcribe them? Put them through OCR? No thank you. 

What you need is ***Dog Ears***: With one press of your chosen shortcut button (default: long press menu) all of the text on screen is captured, much like a clipping, to the Dog Ear text file for that book, and the page is marked similarly to a bookmark. The entry in the text file includes the chapter title, book percentage to the tenths place, and, when available, the embedded page number or stable page number. 
  
Capture the whole text of a page like a screenshot, in a text file like a clipping, marking the page for easy return like a bookmark, all with one quick action, that can be mapped to your button of choice like any other quick action. 

### Reading

- **IBM Plex Serif** is the built-in reader and UI font, with a semibold body for e-ink contrast and IPA and Greek coverage. SD-card fonts still work.
- **Embedded Style** (publisher CSS) with less memory pressure: large stylesheets are looked up from the SD cache instead of being held in RAM, chapter link targets are kept on the card while a chapter is indexed, and a chapter build that runs short of memory restarts itself instead of stopping with "Chapter needs too much memory".
- **Inline images** inside text (from CrossInk v1.6.1).
- **Long footnotes** can be read to the end (previews run up to 24 pages).
- **One percentage format** everywhere (status bars, bookmarks, dog ears, menus, Home), one decimal by default; short books show whole numbers.
- **Go to %** steps of 5% and 10%; a short press of the power button switches to 0.1% and 1%.
- **Landscape Flip** (default: Power + Up) switches between the two landscape orientations; orientation applies to every book.
- **Exempt from Global Orientation** (in-book menu > Book Options, last row): the book keeps its own orientation. Turning it does not change the other books, and changing the orientation elsewhere does not change it. Turn it off to return the book to the global orientation. EPUB books only.
- **Clean Screen** action: three black-and-white refreshes to clear ghosting, then the page is redrawn.
- Default margins 30 (top and bottom) and 35 (left and right); default orientation landscape counter-clockwise.

### Sleep screens

With **Settings > Display > Sleep Screen** set to **Page Overlay**, the current screen stays visible and a picture is drawn over it: black covers, white lets the page show through. Jester picks the picture from a different folder depending on what is on screen when the device goes to sleep:

| Folder (root of the SD card) | Used when |
|---|---|
| `/PortraitSleep` | a book is open in portrait (or inverted portrait) |
| `/LandscapeSleep` | a book is open in either landscape orientation |
| `/HomeSleep` | no book is open (Home, Library, Settings) |

To set it up:

1. Create the folders you want at the root of the SD card. Folder names are not case-sensitive.
2. Put images in them: 480 x 800 BMP files (1-bit works well; PNG also works in Page Overlay). Each sleep picks one at random and avoids repeating the most recent ones.
3. Draw every image as if the device is held upright, buttons at the bottom, because overlays are always drawn that way. For `/LandscapeSleep`, design the picture turned to match how you hold the device in landscape.
4. Choose **Page Overlay** as the sleep screen.

If a folder is missing or empty, Jester uses the normal sleep folder (the one chosen with **Set as Sleep Folder** in the File Browser, otherwise `/.sleep` or `/sleep`). Exception: with no images in `/HomeSleep`, sleeping outside a book shows the light Jester sleep screen. A picture pinned with **Set Sleep Screen** always takes priority over the folders.

Jester remembers which images each folder holds. Files added through File Transfer or the File Browser are picked up at once. After adding or removing images with a computer, delete the folder `/.crosspoint/sleep-image-index` so the folders are read again.

The Dark and Light sleep screens show the ❍ logo and "Sleeping".

### Marking places

- **Dog Ears** (default: hold Select). Marks the page like a bookmark, shows a folded corner in the status bar, and appends the page's text to `/DogEars/dogears-<book>.txt` with the book percentage, the printed (publisher) or stable page number, and the chapter. Dog ears have their own list in the in-book menu and in Bookmarks & Clippings; the first row opens the book's dog ears text file. Opened from the in-book menu, Back returns to the book and Home keeps showing the book.
- **Bookmarks and clippings** from CrossInk. Create Clipping uses the same buttons and labels as Look Up Word in every orientation. The first row of the clippings list opens `/My Clippings.txt`.
- **Unify Bookmarks** (Settings > System, or the in-book menu): after a book was moved or renamed on a computer, re-attaches its bookmarks, dog ears, clippings, favorite and reading position to the new file (matched by title and author in the Library index).

### Status bars

- Top and bottom status bars (from CrossInk v1.6.1) with Battery, Chapter Page Count, Stable Page Number, Book Percentage and **Publisher Page Number** (the printed page from the book's page markers).
- Default bottom bar: Battery left, Publisher Page Number centre, Book Percentage right.

### Home, Library and Favorites

- **Belacqua**, the default UI theme: the Jester logo under the status bar, then a hero tile with the current book's title, author and progress (no cover), then Favorites, Files, Library, Bookmarks & Clippings, File Transfer and Settings, with Settings just above the button hints. Classic and Lyra remain available.
- **Favorites**: add or remove a book from its long-press menu or the in-book menu; up to 36 books, sorted by recently added, title or author.
- **Library** (from CrossInk v1.6.1) with Jester navigation: all four buttons move through the header controls and books, Back moves to the Switch Library control, hold Back goes Home.
- **Multiple libraries**: make any folder a library from its long-press menu in Files and switch with the Switch Library control. Folders can be excluded from the Library, and **Library Index Limit** keeps only the most recently added books.
- The Library keeps its index across sleep, so it does not rescan after every wake. After adding books with a computer while the device sleeps, use **Library > Refresh**.
- File names, Library titles and Favorites titles wrap to two lines, hyphenating long words.

### Controls

Jester keeps CrossInk's configurable shortcuts and adds front-button long presses.

| Default | Action |
|---|---|
| Hold side Up | Quick Actions |
| Hold side Down | Look Up Word |
| Hold Back | Footnotes |
| Hold Select | Dog Ear |
| Power + Up | Landscape Flip |

- **Front Left and Right long presses** can be assigned any action (Settings > Controls > Front Buttons). Short presses still turn pages.
- Quick Actions can open from any of these, including a front long press.
- In landscape counter-clockwise, menus and lists opened from a book treat the upper front button as Up and label it so.

### System

- **SD card firmware update** (Settings > SD Card Firmware Update) is always kept working.
- **File Transfer** creates a hotspot for the web file manager (from Home it starts the hotspot at once); uploaded files carry the browser's date and time.
- Settings live in `/.crosspoint/jester-settings.json` and are never imported from other firmware, so switching between Jester and CrossInk does not mix settings.
- After the reset button, the device starts on Home instead of reopening the last book, so a book that is slow to open cannot trap you.
- `.txt` files with long lines (such as dog-ear files) open quickly.

---

## How Jester differs from CrossInk v1.6.1

### Kept as in CrossInk v1.6.0

These v1.6.1 changes were deliberately not taken:

- **In-book menu**: the X4 v1.6.0 in-book menu and Reader Options, not v1.6.1's unified five-tab reader menu or its full-screen reader menus.
- **Menu navigation**: v1.6.0 button behaviour in menus; v1.6.1's "menu buttons follow the physical layout" change is not used.
- **Footnotes**: the v1.6.0 footnote list, not selecting footnotes on the reading page.
- **Power button**: v1.6.0 sleep and wake behaviour, not v1.6.1's separate Sleep/Wake options.
- **Keyboard**: the v1.6.0 button keyboard, without v1.6.1's layout, spacing and side-hint changes.
- No Home/Reader shortcut, no drawer gear menu, no font previews in global settings.

### Taken from CrossInk v1.6.1 and later

- Library, top and bottom status bars, side-button actions, inline images, and many reader, sleep, clippings and File Browser fixes.
- From CrossInk development after v1.6.1: the page-turn crash fix at the end of a book (#822) and per-fragment layout memory checks.

### Removed

Removed features are hidden and switched off; their saved values are kept reserved.

- Reading statistics, time left, focus (bionic) reading, guide dots and auto page turn.
- Wi-Fi networks, KOReader Sync, OPDS, Check for Updates, Calibre Wireless and Nearby transfers.
- Publisher page numbers in the page margin (they appear as a status bar item instead).
- UI themes other than Classic, Lyra and Belacqua; sleep screens other than None, Dark, Light, Custom, Page Overlay and Quick Resume.
- CrossInk's reader fonts (Lexend Deca, Bitter) and non-English UI languages.

See [CHANGELOG.md](./CHANGELOG.md) for the full list.

---

## Tips for the best reading experience

Jester runs on an ESP32-C3 with about 380 KB of RAM.

- Keep folders under about 200 files; 50-100 per folder browses fastest. Split large collections into folders by author, series or status.
- Text-first EPUBs work best. Very large, image-heavy or omnibus EPUBs can be slow or run short of memory. EPUBs under 20 MB are the most comfortable.
- If a book is slow, optimize it with the web optimizer in File Transfer before copying it.
- Use a reliable SD card and leave free space: settings, progress, caches and dog-ear files live on the card.
- After a firmware update that changes the layout, books re-index on first open; clearing `.crosspoint/epub_*` forces it.

---

## Installation

Download the latest `Jester_v<version>.bin` (for example `Jester_v1.0.2.bin`) first.

### With the CrossPoint web flash tool (USB)

1. Connect the X4 to your computer with a USB cable.
2. Open the flash tool at [crosspointreader.com/#flash-tools](https://crosspointreader.com/#flash-tools).
3. Select **XTEINK X4**, choose the custom `.bin` option, pick the Jester `.bin`, and flash.

### From the SD card

1. Copy the `.bin` to the SD card.
2. On a device running CrossInk or Jester, open **Settings > SD Card Firmware Update** and choose the file.

After an update that changes default settings, delete `/.crosspoint/jester-settings.json` if you want the new Jester defaults.

---

## Development

Jester uses PlatformIO. Initialize the submodules first:

```sh
git submodule update --init --recursive
pio run -e default
```

The firmware is written to `.pio/build/default/Jester_v<version>.bin`; the image must stay under 6,553,600 bytes. The version is `version` in the `[crossink]` section of `platformio.ini`; the device shows it as `Jester_v<version>`. An optional `jester_build = <N>` there adds `-build<N>` for test builds.

See [Getting Started](./docs/development/getting-started.md) and [Testing and Debugging](./docs/development/testing-debugging.md) for the shared CrossInk development guides.

---

## Credits

Jester builds on the work of these projects. All are MIT-licensed unless noted.

- **[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)** by Dave Allie and contributors: the original firmware this lineage comes from.
- **[CrossInk](https://github.com/uxjulia/CrossInk)** by Julia Nguyen: the base of Jester (v1.6.0, with selected v1.6.1 and later changes).
- **[FreeInk SDK](https://github.com/Free-Ink/freeink-sdk)**: display, input, storage and power drivers.
- **[witchhunt-reader](https://github.com/jpirnay/witchhunt-reader)** by Jens Pirnay: streaming chapter anchors to the SD card (`d4547ae6`), and the ideas behind Unify Bookmarks (its global bookmark catalogue) and the Publisher Page Number item (printed-page labels).
- **[inx](https://github.com/obijuankenobiii/inx)**: the on-demand, versioned book-state file that Favorites is modelled on.
- **[casper](https://github.com/tweakerinc/casper)**: lessons on capturing a page's text safely (dog ears) and on avoiding wasted Home work.
- **[YACP](https://github.com/Sichroteph/YACP)** by Sichroteph, via CrossInk: no-op write suppression and per-fragment layout checks.
- **[IBM Plex Serif](https://github.com/IBM/plex)** (SIL Open Font License 1.1).

Licensed under the MIT License, like CrossPoint Reader and CrossInk.
