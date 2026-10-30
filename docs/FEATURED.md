# Featured in

Where Ivy AI has been written about. This table is the ONLY source for the
Featured tab on the site: `tools/gen_site.py` reads it, so add a row here and
regenerate rather than editing the page. Dates are publication dates. The
"What they said" column is a short paraphrase in our words, not a quotation,
because none of these outlets had a board in hand -- every piece was written
from the README and the build log, and the page says so.

Links rot and articles get edited, so each row also carries a screenshot of
the page as it was on the day we took it, under `site/assets/featured/`. The
image is cropped to the top of the article (masthead, headline, byline),
960 by 600, JPEG. Leave the Image cell empty when there is no capture yet and
the card renders without one; the generator fails if a named file is missing.

## Coverage

| Date | Outlet | Title | Link | Image | What they said |
|---|---|---|---|---|---|
| 2026-10-28 | The AI Vibe | Three ESP32 Builds Show How Far On-Device AI Has Come in a Week | https://theaivibe.org/edge/three-esp32-builds-show-how-far-on-device-ai-has-come-in-a-week-2026-10-28 | featured/aivibe.jpg | Prateek Singh's edge-AI dispatch gives Ivy one of its three sections, as proof of how much now runs on one chip, and stays cautious because nobody outside the project has verified the numbers. |
| 2026-10-28 | 开源榜 kaiyuanbang.cn (Chinese) | esp_ai：运行于 ESP32-S3 的离线语音助手 | https://kaiyuanbang.cn/zh-cn/repo/iamankushpandit-esp-ai.html | featured/kaiyuanbang.jpg | A Chinese open-source discovery directory writes up who Ivy suits, how hard it is to deploy and where it stops. Undated; first seen 28 October. |
| 2026-10-25 | BlockseBlock (LinkedIn) | Ivy AI: 8 MB. Take turns. | https://www.linkedin.com/posts/ivy-ai-8-mb-take-turns-ugcPost-7520077602032906240-ub8x/ | featured/linkedin_blockseblock.jpg | A ten-page manga explainer of how speech recognition and the language model share 8 MB by taking turns, careful to call the speed figures the developer's, not a reproduced benchmark. |
| 2026-10-24 | BigTon News (Brazil, Portuguese) | Esta semana no open source em 23 de outubro de 2026 | https://bigton.com.br/noticia.php?slug=esta-semana-no-open-source-em-23-de-outubro-de-2026 | featured/bigton.jpg | A Portuguese summary of the Google roundup, with Ivy under "compact models and offline AI". |
| 2026-10-23 | Google Open Source Blog | This Week in Open Source for October 23, 2026 | https://opensource.googleblog.com/2026/10/this-week-in-open-source-for-october-23.html | featured/googleoss.jpg | Daryl Ducharme's weekly roundup from Google's Open Source Programs Office picks up the story and asks what other everyday devices could run entirely offline. |
| 2026-10-23 | World Programming Society (Hong Kong) | This Week in Open Source for October 23, 2026 | https://www.worldprogramming.org/posts/this-week-in-open-source-for-october-23-2026-i9diy1 | featured/worldprogramming.jpg | A mirror of the Google roundup on a Hong Kong programming community site, shared on Threads by 香港編程學會. |
| 2026-10-23 | Google Open Source on Bluesky | This Week in Open Source #20 is live | https://bsky.app/profile/opensource.google/post/3myksylagcs2n | featured/bsky_googleoss.jpg | Google's own share of the roundup picks three items to name, and "offline ESP32-S3 voice AI" is one of them. |
| 2026-10-19 | 小墨同学 on X (Chinese) | 一块 ESP32，也能跑一整套离线语音助手 | https://x.com/xiaomovps/status/2112109068584473072 | featured/x_xiaomovps.jpg | A long post for a Chinese-speaking audience on what you need, the memory problem and the 63% score. Six and a half thousand views, and where the Chinese-language forks came from. |
| 2026-10-19 | esp32.diy | Ivy AI: Fully Offline Voice Assistant on ESP32-S3 | https://esp32.diy/esp-ai/ | featured/esp32diy.jpg | A five-minute project write-up with a parts list, built from the README. |
| 2026-10-19 | raullenchai/esp32-thinks | ESP32 community survey and feature ideas | https://github.com/raullenchai/esp32-thinks/blob/main/docs/IDEAS.md | featured/esp32thinks.jpg | A 250-star voice-assistant project cites Ivy as the reference for fully offline voice in its ideas document. |
| 2026-10-15 | DEEO AI (YouTube, French) | Veille IA, 12 → 14 oct. | https://www.youtube.com/watch?v=gLHy4hVMHh0 | featured/deeo.jpg | A French AI-news roundup gives Ivy its own three-minute chapter, and is the one outlet so far to spell the author's name right. |
| 2026-10-13 | NEURAL DROP (Instagram) | A whole voice assistant now runs on one microcontroller, with no cloud | https://www.instagram.com/neuraldrop.aitech/reel/Deb-lCBjc38/ | featured/ig_neuraldrop.jpg | A reel summarising the pipeline, with "the catch" stated plainly: 5.4 to 5.9 tokens per second. |
| 2026-10-13 | NEURAL DROP (YouTube Shorts) | A full voice AI running offline on one tiny chip | https://www.youtube.com/shorts/bDJTXXeUgbQ | featured/yt_neuraldrop.jpg | The same script as the reel, as a Short. |
| 2026-10-13 | Voice AI Space | esp_ai | https://www.voiceaispace.com/knowledge/repo/esp-ai | featured/voiceaispace.jpg | A knowledge-hub entry with a plain-language explainer for non-technical readers. |
| 2026-10-12 | Wisevoter | Developer Released Ivy AI for ESP32-S3 Microcontrollers | https://wisevoter.com/world/2026/10/12/ivy-ai-esp32-s3-release | featured/wisevoter.jpg | The earliest piece of the wave, filed before dawn on 12 October, framed around privacy and children. |
| 2026-10-12 | Jay's AI (podcast) | Daily Brief, 12 October 2026 | https://rss.jaysserver.ai/ | featured/jaysserver.jpg | A one-line segment at 5:49 in an AI-hosted daily briefing: a full offline voice AI pipeline on a twenty-dollar board. |
| 2026-10-12 | Open Source For You | Open-Source Voice AI Runs Entirely On ESP32-S3 | https://www.opensourceforu.com/2026/10/open-source-voice-ai-runs-entirely-on-esp32-s3/ | featured/osfy.jpg | A walk through the whole pipeline and the licensing caveats, and the piece most readers arrived from. |
| 2026-10-12 | Electronics For You | This ESP32 Board Runs An Offline AI Tutor For Kids | https://www.electronicsforu.com/news/this-esp32-board-runs-an-offline-ai-tutor-for-kids | featured/efy.jpg | Framed it as a kids' tutor and, fairly, led with the limits: the 14–17 second turn, English only, a US geography syllabus. |
| 2026-10-12 | Adafruit blog | Open-Source Voice AI runs entirely on ESP32-S3 | https://blog.adafruit.com/2026/10/12/open-source-voice-ai-runs-entirely-on-esp32-s3/ | featured/adafruit.jpg | A pick-up of the Open Source For You story, with the pipeline, the board and the speed figures restated. |
| 2026-10-12 | MechaNoticias (Chile) | Ivy AI, un tutor de voz sin internet que corre en un ESP32-S3 | https://noticias.mechatronicstore.cl/electronica/ivy-ai-un-tutor-de-voz-sin-internet-que-corre-en-un-esp32-s3 | featured/mecha.jpg | The Electronics For You piece in Spanish, with a note that the translation was machine-assisted and checked by hand. |
| 2026-09-19 | HardwareLab | esp_ai: fully offline speech, LLM and TTS on one ESP32-S3 | https://hardwarelab.dev/projects/esp-ai-offline/ | featured/hardwarelab.jpg | A project card that indexed the repository the day it was created, before anyone had been told about it. |
| 2026-09-20 | r/esp32 | I built an offline voice assistant that runs the whole pipeline on an ESP32-S3 | https://www.reddit.com/r/esp32/comments/1wlyum9/i_built_an_offline_voice_assistant_that_runs_the/ | featured/reddit.jpg | The launch thread. The engineering landed; the sharpest replies were about disclosing the AI assistance, which is why the README now says so. |

Adafruit and Reddit put a bot check in front of any automated browser, so
those two were captured by hand from a signed-in browser and cropped to the
article column. The X and Instagram captures are the logged-out view of the
post, cropped to the post itself.

## Derived work

| Date | Who | What | Link |
|---|---|---|---|
| 2026-10-07 | iamtilakpatel | Pundit Brain, a fork that reuses the pipeline for an offline wearable note-taker and is porting the board layer to Waveshare ESP32-S3 hardware | https://github.com/iamtilakpatel/pundit-brain |

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit -->

---

*Part of [Ivy AI](https://github.com/iamankushpandit/esp_ai) by [iamankushpandit](https://github.com/iamankushpandit). Copyright © 2026 iamankushpandit, licensed [GPL-3.0-or-later](https://github.com/iamankushpandit/esp_ai/blob/main/LICENSE) alongside the code — reuse of this document, in whole or in part, must keep this attribution and stay under the same licence. See [NOTICE.md](https://github.com/iamankushpandit/esp_ai/blob/main/NOTICE.md).*
