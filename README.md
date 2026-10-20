# Jester

> **Jester is a personal fork of [CrossInk](https://github.com/uxjulia/CrossInk)** (itself a fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)) for the original **Xteink X4** (ESP32-C3, PlatformIO environment `default`). It is based on CrossInk v1.6.0, takes selected changes from CrossInk v1.6.1, and keeps the v1.6.0 menus and navigation.

Current version: **1.0.4** (shown on the device as Jester_v1.0.4).

Jester is a reading-first firmware: fewer features, but the ones that matter for reading books and keeping track of passages in them work well on a 380 KB device. Code for the other CrossInk devices stays in the tree but is not built or supported.

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

- **Jester Serif**, the built-in reader and UI font: IBM Plex Serif with a Medium body for e-ink contrast, plus IPA and Greek. SD-card fonts in `/fonts` still work.
- **Embedded Style** (publisher CSS) with less memory pressure; a chapter that runs short of memory rebuilds itself instead of stopping.
- **Inline images**, and **long footnotes** readable to the end (previews up to 24 pages).
- **One percentage format** everywhere, one decimal by default; short books show whole numbers. **Go to %** steps by 5% and 10%, or by 0.1% and 1% after a short power press.
- **Landscape Flip** switches between the two landscape orientations. Orientation applies to every book unless a book is **Exempt from Global Orientation** (Book Options, last row; EPUB only).
- **Clean Screen**: three black-and-white refreshes clear ghosting, then the page is redrawn.

### Marking places

- **Dog Ears** (above): the page is marked, a folded corner shows in the status bar, and the text is added to `/DogEars/dogears-<book>.txt`. Dog ears have their own list in the in-book menu and in Bookmarks & Clippings; its first row opens the text file.
- **Bookmarks and clippings** from CrossInk. Create Clipping uses the Look Up Word buttons; the first row of the clippings list opens `/My Clippings.txt`.
- **Unify Bookmarks** (Settings > System, or the in-book menu) re-attaches bookmarks, dog ears, clippings, favorite and reading position to a book that was moved or renamed on a computer.

### Sleep screens

The default sleep screen, **Page Overlay**, leaves the screen as it was and draws a picture over it. The pictures made for Jester mimic redacted documents: inside a book, the page you were reading goes to sleep under black bars, like a declassified file. Covering much of the page in solid black also helps reduce ghosting.

Jester picks a picture at random from a folder at the root of the SD card:

| Folder | Used when |
|---|---|
| `/PortraitSleep` | a book is open in portrait |
| `/LandscapeSleep` | a book is open in landscape |
| `/HomeSleep` | no book is open (Home, Library, Settings) |

- Use 480 x 800 BMP (1-bit works well) or PNG, drawn as if the device is held upright; turn landscape pictures to match how you hold the device. White in a BMP lets the screen show through; a PNG without transparency covers it in black and white.
- The `Jester_SD_SleepScreens` download on the Releases page has redacted pictures for `/PortraitSleep` and `/LandscapeSleep`, and Jester's Light and Dark screens in `DarkLight sleep`.
- With no images in `/HomeSleep`, Jester shows its light sleep screen (the ❍ logo and SLEEPING). For the dark one, copy `Jester_Sleep_Dark.png` to `/HomeSleep`.
- A missing or empty book folder falls back to the normal sleep folder. A picture pinned with **Set Sleep Screen** always wins. After changing images with a computer, delete `/.crosspoint/sleep-image-index`.

### Status bars

Top and bottom bars with Battery, Chapter Page Count, Stable Page Number, Book Percentage and **Publisher Page Number** (the printed page, from the book's page markers). Default bottom bar: Battery, Stable Page Number, Book Percentage.

### Home, Library and Favorites

- **Belacqua** (default theme): the logo, the current book, then Library, Favorites, Bookmarks & Clippings, File Transfer, Browse Files and Settings. Classic and Lyra remain available.
- **Library** opens on the Sort control. From a book, Back moves to Switch Library, then through the top bar, and leaves after Refresh; hold Back to leave at once. Series and genre lines are off by default (Library settings).
- **Multiple libraries**: make any folder a library from its long-press menu in Browse Files. Folders can be excluded, and **Library Index Limit** (default 200 books) keeps the most recently added. After adding books with a computer, use Refresh.
- **Favorites**: up to 36 books; hold Home to change the sort (recently added, title, author).
- File names and titles wrap to two lines, hyphenating long words.

### Controls

| Default | Action |
|---|---|
| Hold front Left | Look Up Word |
| Hold front Right | Dog Ear |
| Hold Select | Quick Actions |
| Hold Back | Footnotes |
| Hold side Up | Landscape Flip |
| Hold side Down | Create Clipping |
| Power + Up | Refresh Screen |

Quick Actions holds Look Up Word, Dog Ear, Library, Create Hotspot and Clean Screen. Quick-return from Footnotes is off. All of these can be changed in Settings > Controls.

### System

- **SD card firmware update** (Settings > SD Card Firmware Update) always keeps working.
- **File Transfer** starts a hotspot for the web file manager.
- Settings live in `/.crosspoint/jester-settings.json` and are never imported from other firmware.
- The reset button starts on Home, so a book that is slow to open cannot trap you.

---

## Jester, CrossInk v1.6.1 and CrossInk v1.6.0

| Feature | Jester | CrossInk v1.6.1 | CrossInk v1.6.0 |
|---|---|---|---|
| In-book menu | v1.6.0 menu and Book Options | Five-tab reader menu | X4 menu and Reader Options |
| Menu buttons | v1.6.0 behaviour | Follow the device layout | Original behaviour |
| Footnotes | List | Chosen on the page | List |
| Power button | v1.6.0 sleep and wake | Separate Sleep and Wake options | Sleep and wake |
| Keyboard | v1.6.0 keyboard | Wider keys, new spacing | Original keyboard |
| Library | Yes, with multiple libraries | Yes | No (Recent Books) |
| Status bars | Top and bottom, plus Publisher Page Number | Top and bottom | One bar |
| Side-button actions | Short and long per button, plus front long presses | Short and long per button | Layout and one long press |
| Inline images | Yes | Yes | No |
| Dog Ears, Favorites, Unify Bookmarks | Yes | No | No |
| Landscape Flip, Clean Screen, per-book orientation | Yes | No | No |
| Sleep folders for Home, portrait and landscape | Yes | No | No |
| Publisher page numbers | Status bar item | Page margin | Page margin |
| Reading stats, time left, focus reading, guide dots, auto page turn | Removed | Yes | Yes |
| Wi-Fi networks, KOReader Sync, OPDS, Calibre, Nearby, update check, Download Fonts | Removed (File Transfer by hotspot only; no HTTPS library) | Yes | Yes |
| UI themes | Classic, Lyra, Belacqua | All CrossInk themes | All CrossInk themes |
| Reader fonts and UI languages | Jester Serif plus SD fonts; English | CrossInk fonts; many languages | CrossInk fonts; many languages |

Jester also takes fixes from CrossInk development after v1.6.1, such as the page-turn crash at the end of a book (#822) and per-fragment layout memory checks. Removed features are hidden and switched off; their saved values stay reserved. See [CHANGELOG.md](./CHANGELOG.md) for everything.

---

## Tips

- Keep folders under about 200 files; 50-100 browse fastest.
- Text-first EPUBs under 20 MB work best. Very large or image-heavy EPUBs can be slow or run short of memory; the web optimizer in File Transfer can help.
- Use a reliable SD card with free space: settings, progress, caches and dog-ear files live on it.
- After a firmware update that changes the layout, books re-index on first open; deleting `.crosspoint/epub_*` forces it.

---

## Installation

Download the latest `Jester_v<version>.bin`, then either:

- **USB**: open the flash tool at [crosspointreader.com/#flash-tools](https://crosspointreader.com/#flash-tools), select **XTEINK X4**, choose the custom `.bin` option and flash the Jester file; or
- **SD card**: copy the `.bin` to the card and, on a device running CrossInk or Jester, open **Settings > SD Card Firmware Update**.

New default settings apply to a fresh settings file. To get them after an update, delete `/.crosspoint/jester-settings.json`.

---

## Development

```sh
git submodule update --init --recursive
pio run -e default
```

The firmware is written to `.pio/build/default/Jester_v<version>.bin` and must stay under 6,553,600 bytes. The version is `version` in the `[crossink]` section of `platformio.ini`; an optional `jester_build = <N>` adds `-build<N>` for test builds. See [Getting Started](./docs/development/getting-started.md) and [Testing and Debugging](./docs/development/testing-debugging.md) for the shared CrossInk guides.

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
- **[IBM Plex Serif](https://github.com/IBM/plex)** by IBM (SIL Open Font License 1.1): the source of the built-in Jester Serif. The licence reserves the name "Plex", so the converted font carries another name; it is not endorsed by IBM.
- **[Noto Serif](https://github.com/notofonts/latin-greek-cyrillic)** and **Noto Sans Symbols 2** (SIL Open Font License 1.1): Greek and IPA characters in Jester Serif, and UI symbols.

Licensed under the MIT License, like CrossPoint Reader and CrossInk. The firmware also contains third-party components under their own licences, listed with their licence texts in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
