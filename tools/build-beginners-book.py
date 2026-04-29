#!/usr/bin/env python3
"""Build the KernelESP beginner book PDF.

The book is intentionally generated from code instead of hand-authored PDF
pages. That keeps the result reproducible and makes future updates cheap when
the firmware or web UI changes.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from textwrap import wrap
from typing import Callable

from reportlab.lib import colors
from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "book" / "KernelESP_Beginners_Book.pdf"
TITLE = "KernelESP"
SUBTITLE = "A Beginner's Guide to a Tiny UNIX for the ESP8266"
VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()


PALETTE = {
    "ink": colors.HexColor("#17202A"),
    "muted": colors.HexColor("#5E6C76"),
    "paper": colors.HexColor("#F8F3EA"),
    "cream": colors.HexColor("#FFF9EE"),
    "blue": colors.HexColor("#1F6FEB"),
    "teal": colors.HexColor("#11A39A"),
    "green": colors.HexColor("#2E7D32"),
    "orange": colors.HexColor("#F28C28"),
    "red": colors.HexColor("#C2410C"),
    "dark": colors.HexColor("#0B1220"),
    "code": colors.HexColor("#111827"),
    "line": colors.HexColor("#D9CDBB"),
    "soft_blue": colors.HexColor("#E9F2FF"),
    "soft_green": colors.HexColor("#E9F7EF"),
    "soft_orange": colors.HexColor("#FFF1DE"),
}


@dataclass(frozen=True)
class Topic:
    title: str
    goal: str
    mental_model: str
    commands: tuple[str, ...]
    try_this: str
    why_it_matters: str
    mistake: str
    diagram: str = "chip"
    project: str = ""


def clean(text: str) -> str:
    return " ".join(text.strip().split())


class Book:
    def __init__(self, path: Path):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.path = path
        self.c = canvas.Canvas(str(path), pagesize=letter)
        self.w, self.h = letter
        self.page = 0
        self.part = ""
        self.section = ""

    def save(self) -> None:
        self.c.save()

    def show_page(self) -> None:
        self.c.showPage()

    def new_page(self, section: str = "") -> None:
        if self.page:
            self.show_page()
        self.page += 1
        if section:
            self.section = section
        self.c.setFillColor(PALETTE["cream"])
        self.c.rect(0, 0, self.w, self.h, fill=1, stroke=0)
        self.draw_header_footer()

    def draw_header_footer(self) -> None:
        c = self.c
        c.setStrokeColor(PALETTE["line"])
        c.setLineWidth(0.5)
        c.line(0.65 * inch, self.h - 0.55 * inch, self.w - 0.65 * inch, self.h - 0.55 * inch)
        c.line(0.65 * inch, 0.48 * inch, self.w - 0.65 * inch, 0.48 * inch)
        c.setFillColor(PALETTE["muted"])
        c.setFont("Helvetica", 7.8)
        c.drawString(0.72 * inch, self.h - 0.42 * inch, TITLE)
        if self.section:
            c.drawCentredString(self.w / 2, self.h - 0.42 * inch, self.section[:70])
        c.drawRightString(self.w - 0.72 * inch, 0.28 * inch, str(self.page))

    def cover(self) -> None:
        self.page += 1
        c = self.c
        c.setFillColor(PALETTE["dark"])
        c.rect(0, 0, self.w, self.h, fill=1, stroke=0)
        for i, color in enumerate([PALETTE["blue"], PALETTE["teal"], PALETTE["orange"]]):
            c.setFillColor(color)
            c.setFillAlpha(0.18)
            c.circle((1.2 + i * 2.2) * inch, (8.3 - i * 0.75) * inch, (1.9 - i * 0.25) * inch, fill=1, stroke=0)
        c.setFillAlpha(1)
        self.draw_chip(3.5 * inch, 4.55 * inch, 1.15, dark=True)
        c.setFillColor(colors.white)
        c.setFont("Helvetica-Bold", 48)
        c.drawCentredString(self.w / 2, 3.15 * inch, TITLE)
        c.setFont("Helvetica", 17)
        c.setFillColor(colors.HexColor("#D9E6FF"))
        c.drawCentredString(self.w / 2, 2.72 * inch, SUBTITLE)
        c.setFont("Helvetica", 10)
        c.setFillColor(colors.HexColor("#B9C7D6"))
        c.drawCentredString(self.w / 2, 1.2 * inch, f"Version {VERSION} - Modern beginner edition")
        c.setFont("Helvetica-Bold", 9)
        c.drawCentredString(self.w / 2, 0.92 * inch, "Serial shell - Web UI - Automation - Hardware - Recovery - Real projects")

    def title_page(self) -> None:
        self.new_page("Welcome")
        self.h1("KernelESP Beginner's Book", 1.2 * inch)
        self.p(
            "This book is written for people who are new to microcontrollers, new to "
            "embedded web interfaces, or simply want a calm path into KernelESP. "
            "It treats the ESP8266 as a tiny UNIX-like machine: it has commands, "
            "files, logs, scripts, cron jobs, relays, sensors, inputs, mail alerts "
            "and a web interface that keeps everything visible."
        )
        self.callout(
            "How to read it",
            "Start at Part 1 if the board is new to you. Jump to the projects in "
            "Part 8 when you want irrigation, alarms or scheduled actions. Keep "
            "Part 10 nearby when something goes wrong.",
            PALETTE["soft_blue"],
        )
        self.diagram_system_map(1.1 * inch, 2.0 * inch, 4.25 * inch, 2.25 * inch)

    def copyright_page(self) -> None:
        self.new_page("About This Book")
        self.h2("About this edition")
        self.p(
            "This generated edition is part of the KernelESP repository. It is not "
            "a replacement for the command reference; it is a guided tour that "
            "explains why each feature exists, when to use it and how to test it "
            "safely on real hardware."
        )
        self.p(
            "The examples prefer 24-hour time, non-blocking timers and explicit "
            "recovery paths. That style matters on a small ESP8266: clarity is a "
            "safety feature, and short commands keep the serial console and web UI "
            "responsive."
        )
        self.code(
            [
                "help",
                "health",
                "free",
                "wifi status",
                "crontab -l",
                "relay status",
            ]
        )

    def toc(self, parts: list[tuple[str, list[str]]]) -> None:
        self.new_page("Table of Contents")
        self.h1("Table of Contents", 0.95 * inch)
        y = self.y
        for idx, (part, chapters) in enumerate(parts, start=1):
            if y < 1.25 * inch:
                self.new_page("Table of Contents")
                y = self.y
            self.c.setFillColor(PALETTE["blue"])
            self.c.setFont("Helvetica-Bold", 13)
            self.c.drawString(0.9 * inch, y, f"Part {idx}: {part}")
            y -= 0.22 * inch
            self.c.setFillColor(PALETTE["ink"])
            self.c.setFont("Helvetica", 9.2)
            for chapter in chapters:
                self.c.drawString(1.08 * inch, y, chapter)
                y -= 0.16 * inch
            y -= 0.08 * inch
        self.y = y

    @property
    def y(self) -> float:
        return getattr(self, "_y", self.h - 0.95 * inch)

    @y.setter
    def y(self, value: float) -> None:
        self._y = value

    def h1(self, text: str, y: float | None = None) -> None:
        if y is not None:
            self.y = self.h - y
        self.c.setFillColor(PALETTE["ink"])
        self.c.setFont("Helvetica-Bold", 24)
        self.c.drawString(0.85 * inch, self.y, text)
        self.y -= 0.38 * inch

    def h2(self, text: str) -> None:
        self.c.setFillColor(PALETTE["blue"])
        self.c.setFont("Helvetica-Bold", 15)
        self.c.drawString(0.85 * inch, self.y, text)
        self.y -= 0.27 * inch

    def h3(self, text: str) -> None:
        self.c.setFillColor(PALETTE["ink"])
        self.c.setFont("Helvetica-Bold", 11)
        self.c.drawString(0.9 * inch, self.y, text)
        self.y -= 0.22 * inch

    def p(self, text: str, size: float = 10.2, leading: float = 13.2, x: float = 0.9 * inch, width: int = 86) -> None:
        self.c.setFont("Helvetica", size)
        self.c.setFillColor(PALETTE["ink"])
        for line in wrap(clean(text), width):
            if self.y < 0.85 * inch:
                self.new_page(self.section)
            self.c.drawString(x, self.y, line)
            self.y -= leading
        self.y -= 0.07 * inch

    def bullet(self, items: list[str]) -> None:
        self.c.setFont("Helvetica", 9.8)
        self.c.setFillColor(PALETTE["ink"])
        for item in items:
            lines = wrap(clean(item), 82)
            self.c.setFillColor(PALETTE["teal"])
            self.c.circle(0.98 * inch, self.y + 3, 2.4, fill=1, stroke=0)
            self.c.setFillColor(PALETTE["ink"])
            for idx, line in enumerate(lines):
                self.c.drawString(1.1 * inch, self.y, line)
                self.y -= 12.5
            self.y -= 0.03 * inch

    def code(self, lines: tuple[str, ...] | list[str], title: str = "Try it") -> None:
        box_h = (len(lines) + 2.2) * 0.19 * inch
        if self.y - box_h < 0.95 * inch:
            self.new_page(self.section)
        x = 0.9 * inch
        w = self.w - 1.8 * inch
        y = self.y - box_h
        self.c.setFillColor(PALETTE["code"])
        self.c.roundRect(x, y, w, box_h, 9, fill=1, stroke=0)
        self.c.setFillColor(colors.HexColor("#9FE6C8"))
        self.c.setFont("Helvetica-Bold", 8.5)
        self.c.drawString(x + 0.18 * inch, y + box_h - 0.25 * inch, title)
        self.c.setFillColor(colors.HexColor("#F3F4F6"))
        self.c.setFont("Courier", 8.2)
        yy = y + box_h - 0.48 * inch
        for line in lines:
            self.c.drawString(x + 0.22 * inch, yy, line[:96])
            yy -= 0.17 * inch
        self.y = y - 0.18 * inch

    def callout(self, title: str, text: str, fill: colors.Color = PALETTE["soft_orange"]) -> None:
        lines = wrap(clean(text), 78)
        box_h = (len(lines) + 1.6) * 0.18 * inch
        if self.y - box_h < 0.9 * inch:
            self.new_page(self.section)
        x = 0.9 * inch
        y = self.y - box_h
        self.c.setFillColor(fill)
        self.c.roundRect(x, y, self.w - 1.8 * inch, box_h, 10, fill=1, stroke=0)
        self.c.setFillColor(PALETTE["ink"])
        self.c.setFont("Helvetica-Bold", 9.8)
        self.c.drawString(x + 0.18 * inch, y + box_h - 0.25 * inch, title)
        self.c.setFont("Helvetica", 8.9)
        yy = y + box_h - 0.45 * inch
        for line in lines:
            self.c.drawString(x + 0.18 * inch, yy, line)
            yy -= 0.16 * inch
        self.y = y - 0.16 * inch

    def part_page(self, number: int, title: str, promise: str, diagram: str) -> None:
        self.new_page(f"Part {number}: {title}")
        self.c.setFillColor(PALETTE["dark"])
        self.c.roundRect(0.85 * inch, 6.65 * inch, self.w - 1.7 * inch, 2.25 * inch, 18, fill=1, stroke=0)
        self.c.setFillColor(PALETTE["orange"])
        self.c.setFont("Helvetica-Bold", 12)
        self.c.drawString(1.15 * inch, 8.35 * inch, f"PART {number}")
        self.c.setFillColor(colors.white)
        self.c.setFont("Helvetica-Bold", 25)
        self.c.drawString(1.15 * inch, 7.85 * inch, title)
        self.c.setFillColor(colors.HexColor("#D9E6FF"))
        self.c.setFont("Helvetica", 12)
        for idx, line in enumerate(wrap(clean(promise), 68)):
            self.c.drawString(1.15 * inch, (7.45 - idx * 0.22) * inch, line)
        self.y = 6.05 * inch
        self.draw_named_diagram(diagram, 1.15 * inch, 2.15 * inch, 4.2 * inch, 2.7 * inch)
        self.callout("Beginner promise", "You do not have to memorize this part. Run the commands, observe the output, and build confidence one small success at a time.", PALETTE["soft_green"])

    def topic_pair(self, idx: int, topic: Topic) -> None:
        self.new_page(topic.title)
        self.h1(f"{idx}. {topic.title}", 0.95 * inch)
        self.p(topic.goal)
        self.draw_named_diagram(topic.diagram, 1.05 * inch, 4.55 * inch, 4.6 * inch, 1.85 * inch)
        self.y = 4.15 * inch
        self.h2("The mental model")
        self.p(topic.mental_model)
        self.callout("Why this matters", topic.why_it_matters, PALETTE["soft_blue"])
        self.h3("Common beginner mistake")
        self.p(topic.mistake, size=9.5, leading=12.5)

        self.new_page(topic.title + " - Lab")
        self.h1(f"Lab {idx}: {topic.title}", 0.95 * inch)
        self.p(
            "The goal of this lab is not speed. Type the commands slowly, read the "
            "output, and keep the serial console available as your rescue path."
        )
        self.code(list(topic.commands), "Commands")
        self.h2("What to check")
        self.bullet(
            [
                "The command should return promptly. If it does not, stop adding new automations and check Wi-Fi, heap and logs.",
                "The output should tell you what changed. KernelESP is designed to be inspectable, so verify state after every important action.",
                "If the command controls hardware, test first with dryrun or without mains voltage connected.",
            ]
        )
        if topic.project:
            self.callout("Project step", topic.project, PALETTE["soft_green"])
        self.callout("Try this next", topic.try_this, PALETTE["soft_orange"])

    def command_atlas_page(self, title: str, commands: list[str], explanation: str) -> None:
        self.new_page("Command Atlas")
        self.h1(title, 0.95 * inch)
        self.p(explanation)
        columns = 2
        x0 = 0.9 * inch
        y0 = self.y
        col_w = (self.w - 1.8 * inch) / columns
        self.c.setFont("Courier", 8.3)
        self.c.setFillColor(PALETTE["ink"])
        for i, command in enumerate(commands):
            col = i % columns
            row = i // columns
            x = x0 + col * col_w
            y = y0 - row * 0.19 * inch
            if y < 1.15 * inch:
                self.new_page("Command Atlas")
                self.h1(title + " (continued)", 0.95 * inch)
                y0 = self.y
                row = 0
                y = y0
            self.c.drawString(x, y, command[:40])
        self.y = 1.0 * inch

    def draw_chip(self, x: float, y: float, scale: float = 1.0, dark: bool = False) -> None:
        c = self.c
        body = colors.HexColor("#111827") if not dark else colors.HexColor("#172554")
        edge = colors.HexColor("#60A5FA") if dark else PALETTE["blue"]
        c.setFillColor(body)
        c.roundRect(x - 1.1 * inch * scale, y - 0.72 * inch * scale, 2.2 * inch * scale, 1.44 * inch * scale, 14 * scale, fill=1, stroke=0)
        c.setFillColor(edge)
        for i in range(8):
            yy = y - 0.55 * inch * scale + i * 0.16 * inch * scale
            c.rect(x - 1.28 * inch * scale, yy, 0.16 * inch * scale, 0.055 * inch * scale, fill=1, stroke=0)
            c.rect(x + 1.12 * inch * scale, yy, 0.16 * inch * scale, 0.055 * inch * scale, fill=1, stroke=0)
        c.setFillColor(colors.white if dark else colors.HexColor("#E5E7EB"))
        c.setFont("Helvetica-Bold", 11 * scale)
        c.drawCentredString(x, y + 0.12 * inch * scale, "ESP8266")
        c.setFont("Helvetica", 7.5 * scale)
        c.drawCentredString(x, y - 0.12 * inch * scale, "KernelESP")

    def draw_named_diagram(self, name: str, x: float, y: float, w: float, h: float) -> None:
        diagrams: dict[str, Callable[[float, float, float, float], None]] = {
            "chip": self.diagram_chip,
            "system": self.diagram_system_map,
            "wifi": self.diagram_wifi,
            "files": self.diagram_files,
            "web": self.diagram_web,
            "cron": self.diagram_cron,
            "relay": self.diagram_relay,
            "sensor": self.diagram_sensor,
            "automation": self.diagram_automation,
            "irrigation": self.diagram_irrigation,
            "security": self.diagram_security,
            "memory": self.diagram_memory,
            "recovery": self.diagram_recovery,
            "api": self.diagram_api,
        }
        diagrams.get(name, self.diagram_chip)(x, y, w, h)

    def diagram_frame(self, x: float, y: float, w: float, h: float, title: str) -> None:
        self.c.setFillColor(colors.white)
        self.c.setStrokeColor(PALETTE["line"])
        self.c.roundRect(x, y, w, h, 12, fill=1, stroke=1)
        self.c.setFillColor(PALETTE["muted"])
        self.c.setFont("Helvetica-Bold", 8)
        self.c.drawString(x + 0.14 * inch, y + h - 0.22 * inch, title)

    def box(self, x: float, y: float, w: float, h: float, text: str, fill=PALETTE["soft_blue"]) -> None:
        self.c.setFillColor(fill)
        self.c.roundRect(x, y, w, h, 8, fill=1, stroke=0)
        self.c.setFillColor(PALETTE["ink"])
        self.c.setFont("Helvetica-Bold", 8.2)
        for idx, line in enumerate(wrap(text, 16)):
            self.c.drawCentredString(x + w / 2, y + h / 2 + (0.06 - idx * 0.14) * inch, line)

    def arrow(self, x1: float, y1: float, x2: float, y2: float) -> None:
        self.c.setStrokeColor(PALETTE["muted"])
        self.c.setLineWidth(1.1)
        self.c.line(x1, y1, x2, y2)
        self.c.setFillColor(PALETTE["muted"])
        self.c.circle(x2, y2, 2.0, fill=1, stroke=0)

    def diagram_chip(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Tiny computer")
        self.draw_chip(x + w * 0.5, y + h * 0.48, 0.62)

    def diagram_system_map(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "KernelESP system map")
        self.box(x + 0.15 * inch, y + h * 0.58, 1.1 * inch, 0.45 * inch, "Serial shell")
        self.box(x + w - 1.25 * inch, y + h * 0.58, 1.1 * inch, 0.45 * inch, "Web UI", PALETTE["soft_green"])
        self.box(x + w * 0.5 - 0.65 * inch, y + h * 0.42, 1.3 * inch, 0.55 * inch, "Command engine", PALETTE["soft_orange"])
        self.box(x + 0.2 * inch, y + 0.25 * inch, 1.1 * inch, 0.45 * inch, "LittleFS")
        self.box(x + w - 1.3 * inch, y + 0.25 * inch, 1.1 * inch, 0.45 * inch, "GPIO")
        self.arrow(x + 1.28 * inch, y + h * 0.69, x + w * 0.5 - 0.68 * inch, y + h * 0.55)
        self.arrow(x + w - 1.28 * inch, y + h * 0.69, x + w * 0.5 + 0.68 * inch, y + h * 0.55)
        self.arrow(x + w * 0.5 - 0.2 * inch, y + h * 0.42, x + 1.25 * inch, y + 0.7 * inch)
        self.arrow(x + w * 0.5 + 0.2 * inch, y + h * 0.42, x + w - 1.25 * inch, y + 0.7 * inch)

    def diagram_wifi(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Wi-Fi path")
        self.box(x + 0.2 * inch, y + 0.7 * inch, 1.05 * inch, 0.5 * inch, "Router")
        self.box(x + w * 0.5 - 0.55 * inch, y + 0.7 * inch, 1.1 * inch, 0.5 * inch, "ESP8266", PALETTE["soft_green"])
        self.box(x + w - 1.25 * inch, y + 0.7 * inch, 1.05 * inch, 0.5 * inch, "Browser")
        self.arrow(x + 1.25 * inch, y + 0.95 * inch, x + w * 0.5 - 0.6 * inch, y + 0.95 * inch)
        self.arrow(x + w * 0.5 + 0.6 * inch, y + 0.95 * inch, x + w - 1.25 * inch, y + 0.95 * inch)
        self.c.setFillColor(PALETTE["teal"])
        for r in [0.22, 0.34, 0.46]:
            self.c.arc(x + w * 0.5 - r * inch, y + 1.05 * inch, x + w * 0.5 + r * inch, y + (1.05 + r * 0.7) * inch, 20, 140)

    def diagram_files(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "LittleFS layout")
        self.c.setFont("Courier", 8)
        self.c.setFillColor(PALETTE["ink"])
        lines = ["/", "  /etc     config, cron, relays", "  /home    scripts and notes", "  /www     web interface", "  /var/log kernel.log", "  /func    persistent functions"]
        yy = y + h - 0.45 * inch
        for line in lines:
            self.c.drawString(x + 0.28 * inch, yy, line)
            yy -= 0.22 * inch

    def diagram_web(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Web UI")
        labels = ["Dashboard", "Live UI", "Commands", "Relays", "Scripts", "Help"]
        for i, label in enumerate(labels):
            col = i % 3
            row = i // 3
            self.box(x + 0.22 * inch + col * 1.25 * inch, y + 0.42 * inch + row * 0.62 * inch, 1.05 * inch, 0.42 * inch, label, [PALETTE["soft_blue"], PALETTE["soft_green"], PALETTE["soft_orange"]][i % 3])

    def diagram_cron(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Cron timeline")
        base_y = y + 0.78 * inch
        self.c.setStrokeColor(PALETTE["blue"])
        self.c.setLineWidth(2)
        self.c.line(x + 0.35 * inch, base_y, x + w - 0.35 * inch, base_y)
        for label, pct in [("00:00", 0.18), ("06:00", 0.38), ("12:00", 0.58), ("18:00", 0.78)]:
            xx = x + pct * w
            self.c.setFillColor(PALETTE["orange"])
            self.c.circle(xx, base_y, 4, fill=1, stroke=0)
            self.c.setFillColor(PALETTE["ink"])
            self.c.setFont("Helvetica", 8)
            self.c.drawCentredString(xx, base_y - 0.25 * inch, label)

    def diagram_relay(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Relay isolation")
        self.box(x + 0.25 * inch, y + 0.72 * inch, 1.0 * inch, 0.48 * inch, "ESP GPIO")
        self.box(x + w * 0.5 - 0.5 * inch, y + 0.72 * inch, 1.0 * inch, 0.48 * inch, "Relay", PALETTE["soft_orange"])
        self.box(x + w - 1.25 * inch, y + 0.72 * inch, 1.0 * inch, 0.48 * inch, "Valve")
        self.arrow(x + 1.25 * inch, y + 0.96 * inch, x + w * 0.5 - 0.55 * inch, y + 0.96 * inch)
        self.arrow(x + w * 0.5 + 0.55 * inch, y + 0.96 * inch, x + w - 1.25 * inch, y + 0.96 * inch)

    def diagram_sensor(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Sensor loop")
        self.box(x + 0.25 * inch, y + 0.8 * inch, 1.0 * inch, 0.45 * inch, "Sensor")
        self.box(x + w * 0.5 - 0.55 * inch, y + 0.8 * inch, 1.1 * inch, 0.45 * inch, "Rule")
        self.box(x + w - 1.25 * inch, y + 0.8 * inch, 1.0 * inch, 0.45 * inch, "Action")
        self.arrow(x + 1.25 * inch, y + 1.02 * inch, x + w * 0.5 - 0.58 * inch, y + 1.02 * inch)
        self.arrow(x + w * 0.5 + 0.58 * inch, y + 1.02 * inch, x + w - 1.25 * inch, y + 1.02 * inch)

    def diagram_automation(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "C-like automation")
        self.box(x + 0.18 * inch, y + 0.9 * inch, 1.0 * inch, 0.42 * inch, "Condition")
        self.box(x + w * 0.5 - 0.5 * inch, y + 0.9 * inch, 1.0 * inch, 0.42 * inch, "Block", PALETTE["soft_green"])
        self.box(x + w - 1.18 * inch, y + 0.9 * inch, 1.0 * inch, 0.42 * inch, "Else")
        self.c.setFont("Courier", 7.4)
        self.c.setFillColor(PALETTE["ink"])
        self.c.drawCentredString(x + w / 2, y + 0.45 * inch, "if (wifi == connected) { logger ok } else { logger down }")
        self.arrow(x + 1.2 * inch, y + 1.1 * inch, x + w * 0.5 - 0.55 * inch, y + 1.1 * inch)
        self.arrow(x + w * 0.5 + 0.55 * inch, y + 1.1 * inch, x + w - 1.2 * inch, y + 1.1 * inch)

    def diagram_irrigation(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Garden zones")
        for i, label in enumerate(["Trees", "Flowers", "Greenhouse"]):
            self.box(x + 0.25 * inch + i * 1.25 * inch, y + 0.95 * inch, 1.0 * inch, 0.42 * inch, label, PALETTE["soft_green"])
            self.c.setStrokeColor(PALETTE["teal"])
            self.c.line(x + 0.75 * inch + i * 1.25 * inch, y + 0.95 * inch, x + 0.75 * inch + i * 1.25 * inch, y + 0.42 * inch)
        self.c.setStrokeColor(PALETTE["blue"])
        self.c.setLineWidth(2)
        self.c.line(x + 0.35 * inch, y + 0.42 * inch, x + w - 0.35 * inch, y + 0.42 * inch)

    def diagram_security(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Safety layers")
        for i, label in enumerate(["Password", "Lockout", "Backup", "Serial rescue"]):
            self.box(x + 0.38 * inch + i * 0.95 * inch, y + 0.75 * inch, 0.82 * inch, 0.5 * inch, label, PALETTE["soft_orange"])

    def diagram_memory(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Memory budget")
        bars = [("RAM", 0.53, PALETTE["teal"]), ("IRAM", 0.95, PALETTE["orange"]), ("Flash", 0.46, PALETTE["blue"])]
        yy = y + h - 0.62 * inch
        for label, pct, color in bars:
            self.c.setFillColor(PALETTE["muted"])
            self.c.setFont("Helvetica", 8)
            self.c.drawString(x + 0.25 * inch, yy + 0.03 * inch, label)
            self.c.setFillColor(colors.HexColor("#E5E7EB"))
            self.c.roundRect(x + 0.95 * inch, yy, 2.6 * inch, 0.14 * inch, 4, fill=1, stroke=0)
            self.c.setFillColor(color)
            self.c.roundRect(x + 0.95 * inch, yy, 2.6 * inch * pct, 0.14 * inch, 4, fill=1, stroke=0)
            yy -= 0.36 * inch

    def diagram_recovery(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "Recovery ladder")
        steps = ["Serial", "safe boot", "disable cron", "restore backup"]
        for i, step in enumerate(steps):
            self.box(x + 0.25 * inch + i * 0.95 * inch, y + 0.55 * inch + i * 0.18 * inch, 0.82 * inch, 0.42 * inch, step, PALETTE["soft_blue"])

    def diagram_api(self, x: float, y: float, w: float, h: float) -> None:
        self.diagram_frame(x, y, w, h, "HTTP API")
        self.box(x + 0.25 * inch, y + 0.8 * inch, 1.0 * inch, 0.45 * inch, "curl")
        self.box(x + w * 0.5 - 0.55 * inch, y + 0.8 * inch, 1.1 * inch, 0.45 * inch, "/api/cmd", PALETTE["soft_green"])
        self.box(x + w - 1.25 * inch, y + 0.8 * inch, 1.0 * inch, 0.45 * inch, "JSON")
        self.arrow(x + 1.25 * inch, y + 1.02 * inch, x + w * 0.5 - 0.58 * inch, y + 1.02 * inch)
        self.arrow(x + w * 0.5 + 0.58 * inch, y + 1.02 * inch, x + w - 1.25 * inch, y + 1.02 * inch)


def topic(title: str, goal: str, model: str, commands: list[str], try_this: str, why: str, mistake: str, diagram: str, project: str = "") -> Topic:
    return Topic(title, goal, model, tuple(commands), try_this, why, mistake, diagram, project)


TOPICS = [
    topic("Meet the Tiny UNIX", "KernelESP turns an ESP8266 into a small networked computer with a shell, files, web pages and automation.", "Think of the board as a tiny server. It cannot run Linux, but it borrows the useful habits: inspect first, change second, save the result.", ["uname", "version", "whoami", "help", "health"], "Run help for three commands you do not understand yet and write down what each one controls.", "A mental model prevents panic. If it has commands, files and logs, you can reason about it.", "Treating the ESP8266 like a black box. KernelESP is inspectable; use that gift.", "system"),
    topic("Serial Console First Contact", "The serial console is the most reliable way to talk to the board, especially during Wi-Fi trouble.", "Serial is the local keyboard and screen. It works before the network is ready and remains the best rescue route.", ["help", "wifi status", "free", "df", "dmesg"], "Unplug Wi-Fi mentally: which commands would still help you recover?", "Every serious installation should keep a serial rescue path available during setup.", "Changing Wi-Fi and then closing the only working console.", "chip"),
    topic("Find the Web Interface", "The web UI gives beginners a friendly control panel for commands, relays, scripts, logs and help.", "The browser is a remote window into the same command engine used by serial.", ["wifi ip", "wifi status", "hostname", "health"], "Open the IP address in a browser and compare Dashboard data with health output.", "Seeing the same state in two places builds confidence that you are controlling the right board.", "Assuming the browser is broken when the board simply changed IP.", "web"),
    topic("Use the HTTP API", "The API lets scripts and other computers ask KernelESP to run commands and return JSON.", "The API is the command line with a URL around it. It is useful for integrations and tests.", ["curl -G http://<ip>/api/cmd --data-urlencode key=admin --data-urlencode c=free", "curl http://<ip>/api/status?key=admin"], "Run an API command from your laptop and compare it with the web command runner.", "APIs are how small devices join bigger systems without needing a cloud service.", "Forgetting URL encoding when a command contains spaces, quotes or symbols.", "api"),
    topic("Read System Health", "Health commands tell you whether the board is stable before you add automation.", "Heap, Wi-Fi state, filesystem space and time are vital signs.", ["health", "free", "uptime", "df", "sysinfo"], "Record heap before and after opening Live UI, then after running a script.", "A baseline makes future debugging factual instead of emotional.", "Ignoring low heap or high fragmentation until the web UI feels slow.", "memory"),
    topic("Understand LittleFS", "KernelESP stores configuration, scripts and web assets in LittleFS.", "LittleFS is the board's small disk. It survives reboot but has limited space.", ["ls /", "ls /etc", "ls /home", "df", "du /"], "Create a tiny note in /home, read it, then delete it.", "Files make automations persistent and inspectable.", "Saving everything in one giant file instead of small named files.", "files"),
    topic("Create and Inspect Files", "The shell can create, append, read, search and remove files.", "File commands are small building blocks for scripts and backups.", ["write /home/hello.txt hello", "append /home/hello.txt world", "cat /home/hello.txt", "grep hello /home/hello.txt", "rm /home/hello.txt"], "Create a checklist file for your installation.", "A beginner who can inspect files can recover from many configuration mistakes.", "Editing a file without reading it first.", "files"),
    topic("Use Pipes Like a Pro", "Pipes connect small commands so one command filters another.", "A pipe is a little conveyor belt: left command produces text, right command narrows it down.", ["health | grep wifi", "dmesg | tail -n 5", "cat /var/log/kernel.log | grep cron", "health | wc -l"], "Find all log lines containing wifi or ntp.", "Pipes make the tiny shell feel surprisingly UNIX-like.", "Using pipes for everything; sometimes one direct command is clearer.", "system"),
    topic("History and Aliases", "History and aliases save typing during repeated setup work.", "An alias is a short nickname for a longer command.", ["history", "alias ll ls /", "ll", "alias save", "history save"], "Create an alias for health and remove it later.", "Shortcuts reduce mistakes when commands are long.", "Making aliases with names that hide real commands.", "chip"),
    topic("Dry Run Mode", "Dry run helps test dangerous commands without energizing hardware.", "It is a rehearsal mode for actuators.", ["dryrun on", "relay on valve1", "relay status", "dryrun off"], "Use dryrun before connecting a pump, valve or mains relay.", "Safety-first habits matter more than clever automation.", "Testing new relay logic directly on live hardware.", "relay"),
    topic("Scan and Connect Wi-Fi", "Wi-Fi commands connect the board while keeping the shell responsive.", "Connection starts in the background; you keep control while it negotiates.", ["wifi scan", "wifi connect MySSID MyPassword", "wifi status", "wifi wait 30"], "Connect, wait, and verify IP address without rebooting.", "Non-blocking Wi-Fi avoids freezing the rescue console.", "Running many network commands while the first connection is still settling.", "wifi"),
    topic("Save Wi-Fi Profiles", "Profiles make it easy to move between networks or recover from credential changes.", "A profile is a named Wi-Fi memory.", ["wifi save HomeNetwork SecretPassword", "wifi profile list", "wifi autoconnect on", "wifi reconnect"], "Save a profile and inspect it before relying on it.", "Installations in gardens and workshops often move during testing.", "Forgetting that saved credentials persist after reboot.", "wifi"),
    topic("Recover Wi-Fi", "Recovery commands help when the board joins the wrong network or gets confused.", "Wi-Fi recovery is a ladder: inspect, reconnect, forget, recover, SDK reset only when needed.", ["wifi diag", "wifi recover", "wifi disconnect", "wifi reconnect", "wifi sdkreset --yes"], "Write a recovery checklist before you need it.", "A calm recovery path prevents unnecessary reflashing.", "Using sdkreset as the first move instead of the last careful step.", "recovery"),
    topic("Static IP or DHCP", "KernelESP can use DHCP or a static address depending on your installation.", "DHCP is convenient; static IP is predictable.", ["wifi dhcp on reconnect", "wifi static 192.168.1.50 192.168.1.1 255.255.255.0 8.8.8.8 1.1.1.1 reconnect", "wifi net"], "Decide which mode is safer for your garden controller.", "Predictable addressing makes web access and API scripts easier.", "Setting a static IP outside the router subnet.", "wifi"),
    topic("Fallback Access Point", "The fallback AP can expose setup access when normal Wi-Fi fails.", "AP mode is a temporary lifeboat, not the normal harbor.", ["ap status", "config get fallback.ap", "config set fallback.ap on", "ap start"], "Confirm whether fallback AP is enabled on your board.", "Remote installations need a way back in after router changes.", "Leaving setup access open without thinking about physical security.", "security"),
    topic("Set the Hostname", "A good hostname makes the board easier to find and recognize.", "Names beat numbers when you have more than one controller.", ["hostname", "hostname kernelesp-garden", "wifi reconnect", "wifi status"], "Choose a name that describes the physical location.", "Clear names prevent editing the wrong controller.", "Using the same hostname for multiple boards.", "wifi"),
    topic("Read the Clock", "Time unlocks cron, logs and safe irrigation windows.", "The clock is the board's calendar. Without it, schedules cannot mean much.", ["date", "date -u", "ntp status", "time status"], "Compare local time and UTC time.", "Every schedule depends on trustworthy time.", "Assuming cron is wrong when NTP never synced.", "cron"),
    topic("Use NTP Kick", "ntp kick starts time synchronization without blocking the rest of the system.", "It is a polite request: start syncing, then give the shell back.", ["ntp kick", "ntp status", "date", "dmesg | tail -n 5"], "Run ntp kick, then keep using the shell while time settles.", "Non-blocking time sync keeps automations responsive.", "Expecting instant perfect time on a weak Wi-Fi network.", "cron"),
    topic("Schedule NTP Twice Daily", "A simple cron pair keeps time fresh without hammering NTP servers.", "Most garden controllers do not need minute-by-minute clock correction.", ["cron add daily 00:00 ntp kick", "cron add daily 12:00 ntp kick", "crontab -l"], "Confirm both jobs appear in cron.", "Twelve-hour sync is a practical balance for ESP8266 projects.", "Scheduling NTP every minute because more feels safer.", "cron"),
    topic("Manual Time Rescue", "Manual date setting is useful when Wi-Fi is down and you still need schedules.", "Manual time is a temporary crutch until NTP returns.", ["date", "date set 2026-04-29 08:30:00", "date", "ntp kick"], "Practice manual time on a bench board before field deployment.", "Time rescue can keep logs understandable during network outages.", "Forgetting to return to NTP after manual testing.", "recovery"),
    topic("Relay Basics", "Relays let a 3.3 V logic pin control a separate load safely when wired correctly.", "The ESP gives a signal; the relay handles the heavier electrical side.", ["relay add valve1 D1 active_low", "relay status", "relay on valve1", "relay off valve1"], "Use dryrun until the relay module behavior is known.", "Relays are the bridge between software and physical action.", "Powering relay coils from the ESP 3.3 V pin.", "relay"),
    topic("Name Relays Well", "Good relay names make scripts readable and safer.", "Names are labels for real-world equipment.", ["relay add trees D1 active_low", "relay add flowers D2 active_low", "relay status", "relay rm flowers"], "Choose names based on the thing controlled, not the GPIO number.", "Readable names reduce mistakes months later.", "Calling everything relay1, relay2 and forgetting which is which.", "relay"),
    topic("Pulse a Relay", "A pulse turns something on briefly and then turns it off automatically.", "Pulse is a short controlled action, useful for tests or momentary loads.", ["relay pulse valve1 500", "relay status", "log | tail -n 5"], "Pulse a test LED before pulsing a real valve.", "Short actions are easier to validate than long schedules.", "Using pulse for long irrigation when a timer would be clearer.", "relay"),
    topic("Timers Instead of Sleep", "Timers schedule future actions without blocking the shell.", "A timer is a reminder the system keeps while you do other things.", ["relay on valve1", "timer once 600000 relay off valve1", "timer list", "relay status"], "Start a short 10 second test timer and watch it complete.", "Non-blocking timers are essential for reliable automation.", "Using sleep for long actuator runs.", "cron"),
    topic("Repeating Timers", "Repeating timers run commands at intervals while the system stays responsive.", "They are useful for lightweight periodic maintenance.", ["timer every 60000 health", "timer list", "timer clear"], "Create a short repeating timer, observe it, then clear it.", "Timers are simple when the interval matters more than the clock time.", "Leaving experimental timers running after tests.", "cron"),
    topic("Cron Daily Jobs", "Cron runs commands at human clock times.", "Cron is a calendar-based scheduler.", ["cron add daily 06:00 relay on trees", "cron add daily 06:10 relay off trees", "crontab -l"], "Create a harmless logger job and remove it.", "Daily cron jobs are perfect for irrigation windows.", "Adding on jobs without matching off or safety timers.", "cron"),
    topic("Cron Weekly Jobs", "Weekly schedules support maintenance and weekly routines.", "The clock decides when; the command engine decides what.", ["cron add weekly mon 08:00 logger monday_check", "cron add weekly sun 22:30 sh /home/weekly.sh", "crontab -l"], "Schedule a weekly health log entry.", "Weekly patterns keep maintenance visible.", "Forgetting local time and using AM/PM habits in a 24-hour system.", "cron"),
    topic("Cron and Safety", "Cron jobs should be short, inspectable and reversible.", "A schedule is only safe if the action is safe.", ["crontab -l", "cron rm <id>", "armed", "disarm", "arm"], "Find the quickest way to pause automations.", "A pause switch is a gift to your future self.", "Debugging a live schedule without disarming it first.", "security"),
    topic("Sensors First Read", "Sensors provide temperature, humidity and pressure values for decisions.", "Sensor values are measurements, not commands. Read before you react.", ["sensor begin", "sensor read", "sensor status", "health"], "Compare sensor output before and after touching the sensor gently.", "Rules and automations are only as good as the sensor data.", "Writing rules before confirming the sensor works.", "sensor"),
    topic("BME280 and BMP280", "Environmental sensors can share I2C wiring when connected correctly.", "I2C is a small data bus: power, ground, clock and data.", ["i2c scan", "sensor begin", "sensor read", "dmesg | tail -n 10"], "Run i2c scan and write down detected addresses.", "Physical wiring errors often look like software bugs.", "Mixing 5 V sensor modules with 3.3 V-only GPIO without checking.", "sensor"),
    topic("Sensor Rules", "Rules turn measurements into actions.", "A rule is a small thermostat-like decision.", ["rule add temp gt 40 relay fan on", "rule list", "rule rm <id>", "relay status"], "Create a logger-only rule before controlling a relay.", "Rules are simpler than scripts for direct threshold behavior.", "Using one threshold for both on and off, causing rapid toggling.", "sensor"),
    topic("Hysteresis", "Hysteresis prevents relays from chattering around a threshold.", "Use one threshold to turn on and another to turn off.", ["if (temp >= 40) relay on fan", "if (temp <= 35) relay off fan", "rule list"], "Choose a gap wide enough for your real environment.", "Mechanical relays and valves should not toggle every few seconds.", "Setting on at 40 and off at 39.9 in noisy conditions.", "sensor"),
    topic("Scenes", "Scenes group commands into named actions.", "A scene is a macro for a state you want often.", ["scene add night relay off lights; logger night_scene", "scene run night", "scene list", "scene show night"], "Create a scene that only logs, then inspect it.", "Scenes make web buttons and scripts easier to understand.", "Putting too much logic inside a scene; use scripts for logic.", "automation"),
    topic("Persistent State", "State stores small values that survive reboot.", "State is the board's notebook.", ["state set irrigation.enabled 1", "state get irrigation.enabled", "state list", "state rm irrigation.enabled"], "Store a harmless value and confirm it persists after reboot later.", "Persistent state lets automations remember modes.", "Using state for large data logs instead of small key values.", "files"),
    topic("Digital Inputs", "Inputs react to buttons, switches and sensor pulses.", "An input is the board listening to a pin.", ["input add rain D2 pullup 50", "input list", "input rm rain"], "Add a named input even before wiring the final sensor.", "Names make input automations readable.", "Forgetting pullup behavior and reading inverted logic.", "automation"),
    topic("Input Events", "Input events attach commands to high, low or change transitions.", "The event is the trigger; the command is the response.", ["input add button D7 pullup 50", "input on button low logger button_pressed", "input list"], "Use logger first, hardware second.", "Logging input events proves your wiring before actuation.", "Starting a pump from an untested bouncing switch.", "automation"),
    topic("C-like Expressions", "The automation language supports familiar comparisons and logic.", "Expressions answer yes or no before running a command.", ["if (wifi == connected) logger online", "if (armed == on && wifi == connected) logger ready", "if (!(armed == on)) logger paused"], "Change armed state and observe which condition fires.", "Readable conditions make complex projects approachable.", "Expecting full C. This is a tiny expression layer, not a compiler.", "automation"),
    topic("Blocks and Else", "Blocks let one condition run several commands.", "A block is a small group of commands separated by semicolons.", ["if (wifi == connected) { logger online; mail health \"online\" } else { logger offline }", "if (armed == on) { relay status; timer list }"], "Keep blocks on one line in scripts.", "Blocks reduce duplicated conditions.", "Writing multiline if blocks in scripts; KernelESP rejects them for stability.", "automation"),
    topic("Variables with let", "let creates persistent variables for automation decisions.", "Variables are readable knobs for behavior.", ["let irrigation.enabled = 1", "let max.temp = 35", "let list", "if (irrigation.enabled == 1 && temp < max.temp) logger allowed"], "Create a variable for your preferred watering duration.", "Variables let beginners tune behavior without rewriting every command.", "Forgetting that variable names are case-insensitive.", "automation"),
    topic("Constants with define", "define creates named constants used in expressions.", "Constants are labels for numbers and times.", ["define HOT 40", "define MORNING_END 10:00", "define list", "if (temp >= HOT && time < MORNING_END) logger hot_morning"], "Replace a magic number in one script with a constant.", "Names explain intent better than bare numbers.", "Assuming HOT and hot are different constants.", "automation"),
    topic("Functions", "Functions store named command blocks under /func.", "A function is a reusable mini script.", ["function water_zone1 { relay on valve1; timer once 600000 relay off valve1; logger zone1_started }", "function list", "function show water_zone1", "call water_zone1"], "Create a function that only logs before using relays.", "Functions keep cron and input actions short.", "Calling functions from inside if branches; use direct commands in branches.", "automation"),
    topic("Scripts", "Scripts are files containing commands, one per line.", "A script is a recipe stored on the board.", ["write /home/test.sh echo start", "append /home/test.sh health", "sh -n /home/test.sh", "sh /home/test.sh"], "Validate scripts with sh -n before running them.", "Scripts make setup repeatable.", "Saving untested scripts into boot.", "files"),
    topic("Boot Script", "The boot script runs startup commands.", "Boot is the board's morning routine.", ["boot show", "boot set /etc/boot.sh", "cat /etc/boot.sh", "onboot list"], "Keep boot commands short and non-blocking.", "A safe boot script makes power loss recovery boring.", "Putting long sleeps or risky relay actions at boot.", "recovery"),
    topic("Logs", "Logs tell the story of what happened.", "When confused, read the diary.", ["log", "tail -n 20 /var/log/kernel.log", "grep cron /var/log/kernel.log", "dmesg"], "Find the last Wi-Fi-related log entry.", "Logs turn mystery failures into timelines.", "Clearing logs before copying the useful clue.", "files"),
    topic("Backups", "Backups preserve configuration before experiments.", "A backup is a snapshot you can return to.", ["backup", "export relays", "cat /etc/config.txt", "ls /etc"], "Make a backup before changing Wi-Fi or cron.", "Small boards are easier to experiment with when rollback exists.", "Testing risky automation without a current backup.", "security"),
    topic("Mail Setup", "Mail alerts let the board report important events.", "Email is an outbound notification path.", ["mail status", "mail config", "mail test", "mail health \"KernelESP health\""], "Send a test email before using mail in automation.", "Notifications are most useful when they are boringly reliable.", "Triggering emails every few seconds from a noisy rule.", "api"),
    topic("Mail Alerts", "A good alert says what happened and what to do next.", "Alerts are for humans, not logs with wings.", ["if (wifi == connected) mail health \"daily health\"", "logger daily_health_sent", "mail status"], "Create one daily health email, not ten noisy alerts.", "Useful alerts make remote installs manageable.", "Sending alert storms during unstable Wi-Fi.", "api"),
    topic("Diagnostics", "Diagnostics bundle the facts needed for support.", "A diagnostic is a snapshot of the system state.", ["diag", "board", "health", "sysinfo", "wifi diag"], "Run diag before and after a major change.", "Good diagnostics reduce guesswork.", "Only reporting symptoms without command output.", "memory"),
    topic("Live UI", "Live UI shows changing system state in the browser.", "It is a dashboard, not a replacement for careful command checks.", ["health", "free", "timer list", "crontab -l"], "Open Live UI and watch heap while running harmless commands.", "Refreshing less often keeps the ESP responsive.", "Leaving many browser tabs polling the board during critical automation.", "web"),
    topic("Command Runner", "The web command runner is a beginner-friendly shell.", "It is ideal for controlled experiments from a browser.", ["echo hello", "free", "relay status", "help relay"], "Run the same command in serial and web and compare outputs.", "Seeing identical behavior builds trust in the model.", "Pasting a long unreviewed command from notes.", "web"),
    topic("Script Editor", "The web editor makes scripts accessible without a serial terminal.", "It is a small file editor for /home and /etc scripts.", ["ls /home", "cat /home/test.sh", "sh -n /home/test.sh"], "Edit a script, validate, save, then run.", "Validation catches simple mistakes before automation does.", "Editing boot scripts directly without backup.", "web"),
    topic("Local Help", "Help files live on the board so documentation is available offline.", "The board carries its own pocket manual.", ["help", "help relay", "ls /help", "cat /help/scripts.txt"], "Open the Help page in the web UI while disconnected from the internet.", "Offline help is valuable in gardens, workshops and basements.", "Assuming online docs are required for every small task.", "web"),
    topic("Security Basics", "Security starts with a good web key and careful network exposure.", "The board should be reachable by you, not by the whole world.", ["config get web.key", "config set web.key <new-key>", "ap status", "wifi net"], "Change the web key on a private bench network first.", "Garden automation can affect real hardware, so access matters.", "Port-forwarding the ESP web UI to the internet.", "security"),
    topic("Web Lockout", "Lockout slows repeated bad password attempts.", "It is a seatbelt, not a castle wall.", ["config get web.lockout", "health", "diag"], "Confirm lockout settings before field installation.", "Even small devices benefit from friction against guessing.", "Depending on lockout while using a weak password.", "security"),
    topic("GPIO Choices", "Not every GPIO is equally safe for relays and sensors.", "Some pins affect boot mode; choose conservative pins first.", ["board pins", "board show", "pin D1", "pin D2"], "Map every wire to a named relay or input.", "Correct pin choice prevents boot surprises.", "Using boot-sensitive pins without understanding pullups.", "relay"),
    topic("External Relay Power", "Relay modules should usually have their own suitable power supply.", "The ESP sends logic; the external supply does the coil work.", ["relay status", "free", "health"], "Measure relay supply voltage while toggling.", "Stable power is more important than elegant code.", "Powering many relay coils from the ESP regulator.", "relay"),
    topic("Long Cable Runs", "Long cables need planning for voltage drop, noise and protection.", "A cable is part of the circuit, not just a line on a drawing.", ["logger cable_test_start", "relay pulse valve1 500", "logger cable_test_done"], "Test with the actual cable length before burying anything.", "Field wiring fails differently from bench wiring.", "Assuming a 100 m cable behaves like a 10 cm jumper.", "relay"),
    topic("Outdoor Enclosures", "Outdoor electronics need protection from water, condensation, sun and frost.", "An enclosure is climate control for a tiny computer.", ["health", "sensor read", "log | tail -n 10"], "Design for drip loops, strain relief and service access.", "Mechanical protection is reliability work.", "Putting a bare relay board in direct sun or frost.", "irrigation"),
    topic("Solenoid Valves", "Irrigation valves are usually better described as solenoids than pumps.", "The relay energizes a valve; water pressure does the work.", ["relay add trees D1 active_low", "relay on trees", "timer once 900000 relay off trees"], "Choose valve voltage before choosing the power supply.", "Solenoids make zone control practical.", "Mixing AC and DC valve assumptions.", "irrigation"),
    topic("Flow Measurement", "A flow sensor can confirm that watering actually happened.", "Relay on means command sent; flow means water moved.", ["input add flow D7 pullup 20", "input on flow change logger flow_pulse", "log | tail -n 20"], "Log pulses before converting them into liters.", "Feedback catches closed taps, clogged filters and broken wires.", "Trusting relay state as proof of water delivery.", "irrigation"),
    topic("Build Zone One", "A first irrigation zone should be simple, observable and reversible.", "One zone is your learning sandbox.", ["relay add trees D1 active_low", "function water_trees { relay on trees; timer once 900000 relay off trees; logger trees_started }", "call water_trees"], "Run the function with water off first.", "A small first success makes the bigger system safer.", "Building four zones before proving one zone.", "irrigation"),
    topic("Add Zone Two", "Second zones teach naming, schedule spacing and power budgeting.", "Zones should not fight for water pressure or relay power.", ["relay add flowers D2 active_low", "function water_flowers { relay on flowers; timer once 300000 relay off flowers; logger flowers_started }", "cron add daily 06:20 call water_flowers"], "Leave time between zones and observe pressure.", "Spacing zones is kinder to plumbing and power supplies.", "Starting every valve at the same minute.", "irrigation"),
    topic("Rain Skip Pattern", "A rain input can pause watering.", "A sensor changes a variable; schedules read that variable.", ["let irrigation.enabled = 1", "input add rain D5 pullup 50", "when input rain low let irrigation.enabled = 0", "let list"], "Use logger first if your rain sensor logic is unknown.", "Separating sensing from watering keeps logic understandable.", "Letting a noisy sensor directly toggle a valve.", "automation"),
    topic("Temperature Window Pattern", "Watering can be restricted by temperature and time.", "Conditions protect plants and hardware.", ["define MAX_TEMP 35", "define MORNING_END 10:00", "if (temp < MAX_TEMP && time < MORNING_END) { relay on trees; timer once 900000 relay off trees }"], "Test the false branch with an impossible threshold.", "A safe window reduces evaporation and heat stress.", "Forgetting that time needs NTP.", "irrigation"),
    topic("Daily Health Report", "A daily report proves the board is alive.", "Heartbeat messages are boring until they save a trip.", ["function daily_health { if (wifi == connected) mail health \"KernelESP daily health\" else logger daily_health_no_wifi }", "cron add daily 08:00 call daily_health"], "Start with logger if mail is not configured.", "Health reports catch silent failures.", "Sending reports too often and training yourself to ignore them.", "api"),
    topic("Recover from Bad Automation", "Bad cron or scripts should be removable without reflashing.", "Recovery means pause, inspect, remove, test.", ["disarm", "crontab -l", "cron rm <id>", "timer clear", "arm"], "Practice disarming before you need it.", "A practiced recovery path makes experimentation safer.", "Trying random fixes while automations are still armed.", "recovery"),
    topic("Serial Rescue Workflow", "Serial rescue is the final trusted path when web access is slow or gone.", "When the network is uncertain, go local.", ["free", "wifi status", "crontab -l", "input list", "reboot"], "Keep a known-good cable and serial command list nearby.", "Many apparent firmware failures are network or script mistakes.", "Reflashing before reading logs.", "recovery"),
    topic("Safe Boot Thinking", "Safe boot keeps startup from becoming a trap.", "Boot should prepare the system, not perform risky work blindly.", ["boot show", "cat /etc/boot.sh", "safe status", "dmesg"], "Review boot commands after every major automation change.", "Power failures happen; startup behavior must be predictable.", "Starting pumps automatically at boot without checks.", "recovery"),
    topic("Firmware Upload", "Uploading firmware updates the program while preserving a disciplined workflow.", "Firmware is the operating brain; assets and files are separate concerns.", ["tools/verify.sh", "tools/upload.sh /dev/cu.usbserial-XXXX", "tools/smoke-http.sh http://<ip> admin"], "Never skip verification on a release candidate.", "A repeatable upload flow avoids mystery states.", "Uploading firmware and forgetting web assets.", "recovery"),
    topic("Asset Upload", "Web assets update the browser interface and help files.", "The firmware serves files from /www and /help.", ["tools/upload-assets.sh http://<ip> admin", "ls /www", "ls /help"], "After asset upload, refresh the browser hard.", "UI fixes may live in files, not firmware.", "Assuming a firmware flash updates every web file.", "web"),
    topic("Release Testing", "A release is not done until checks pass.", "Testing is a ritual that protects future users.", ["tools/verify.sh", "tools/smoke-http.sh http://<ip> admin", "COUNT=60 DELAY=1 tools/stability-http.sh http://<ip> admin"], "Keep test output with your release notes.", "Small devices need boring repeatability.", "Calling it done after only compiling.", "memory"),
    topic("Command Reference Habits", "The command reference is easier when you read by task, not alphabetically.", "Ask: what am I trying to inspect or change?", ["help", "help cron", "help relay", "help scripts"], "Make your own cheat sheet of ten commands.", "Beginners do not need every command at once.", "Trying to memorize the whole shell.", "system"),
    topic("API Integrations", "External systems can call KernelESP without replacing its local control.", "The API is a bridge, not the boss.", ["curl -G http://<ip>/api/cmd --data-urlencode key=admin --data-urlencode c='relay status'", "curl http://<ip>/api/status?key=admin"], "Write one laptop script that reads status only.", "Read-only integration is the safest first step.", "Letting a remote system send actuator commands before local safety exists.", "api"),
    topic("Build a Morning Irrigation Program", "A complete morning program combines time, variables, relays and timers.", "The readable command is the design.", ["let irrigation.enabled = 1", "define WATER_END 10:00", "if (irrigation.enabled == 1 && time < WATER_END) { relay on trees; timer once 900000 relay off trees; logger trees_started } else { logger trees_skipped }"], "Run it manually before scheduling it.", "Manual first, cron second is the safest habit.", "Scheduling a command you have never run manually.", "irrigation"),
    topic("Build a Temperature Fan Program", "Temperature control is a classic automation pattern.", "Sense, decide, act, log.", ["define HOT 40", "define COOL 35", "if (temp >= HOT) { relay on fan; logger fan_on }", "if (temp <= COOL) { relay off fan; logger fan_off }"], "Use a lamp or LED before a real fan.", "The same pattern works for heaters, extractors and alarms.", "Skipping the off condition.", "sensor"),
    topic("Build an Offline Logger", "When Wi-Fi is down, logging still gives evidence.", "Local logs are the fallback memory.", ["if (wifi == connected) { mail health \"online\" } else { logger offline_no_mail }", "tail -n 20 /var/log/kernel.log"], "Force a false condition and confirm the local log path.", "Good automations degrade gracefully.", "Assuming every alert can use the network.", "files"),
    topic("Keep the System Fast", "The ESP8266 web server is small; fewer refreshes and short commands help.", "Responsiveness is a shared resource.", ["free", "health", "ps", "top"], "Close extra Live UI tabs and compare heap/latency.", "A fast UI is often a quiet UI.", "Polling many expensive pages every second.", "memory"),
    topic("Know the Limits", "KernelESP is powerful because it is small, not because it is unlimited.", "Limits are design rails.", ["free", "df", "timer list", "crontab -l", "input list"], "Write down your board's active relays, timers, inputs and cron jobs.", "Knowing limits prevents accidental complexity.", "Treating ESP8266 RAM like a desktop computer.", "memory"),
    topic("Document Your Installation", "A garden controller needs notes as much as code.", "Documentation is part of the system.", ["write /home/INSTALL.txt valve1=trees", "append /home/INSTALL.txt D1=trees relay", "cat /home/INSTALL.txt"], "Store pin and valve notes on the board and in your repo.", "Future maintenance starts with knowing what exists.", "Relying on memory for outdoor wiring.", "files"),
    topic("Final Beginner Checklist", "A safe project is inspected, backed up, tested and recoverable.", "The checklist is your calm co-pilot.", ["health", "free", "df", "crontab -l", "timer list", "input list", "backup"], "Run the checklist before leaving the installation unattended.", "The best automation is one you can explain and recover.", "Leaving a new automation alone without observing one full cycle.", "security"),
]


PARTS = [
    ("Orientation", ["Meet the Tiny UNIX", "Serial Console First Contact", "Find the Web Interface", "Use the HTTP API", "Read System Health"]),
    ("Files and Shell Confidence", ["Understand LittleFS", "Create and Inspect Files", "Use Pipes Like a Pro", "History and Aliases", "Dry Run Mode"]),
    ("Networking and Time", ["Scan and Connect Wi-Fi", "Save Wi-Fi Profiles", "Recover Wi-Fi", "Static IP or DHCP", "Fallback Access Point", "Set the Hostname", "Read the Clock", "Use NTP Kick", "Schedule NTP Twice Daily", "Manual Time Rescue"]),
    ("Hardware Control", ["Relay Basics", "Name Relays Well", "Pulse a Relay", "Timers Instead of Sleep", "Repeating Timers"]),
    ("Scheduling and Sensors", ["Cron Daily Jobs", "Cron Weekly Jobs", "Cron and Safety", "Sensors First Read", "BME280 and BMP280", "Sensor Rules", "Hysteresis"]),
    ("Automation Language", ["Scenes", "Persistent State", "Digital Inputs", "Input Events", "C-like Expressions", "Blocks and Else", "Variables with let", "Constants with define", "Functions", "Scripts", "Boot Script"]),
    ("Web, Mail and Diagnostics", ["Logs", "Backups", "Mail Setup", "Mail Alerts", "Diagnostics", "Live UI", "Command Runner", "Script Editor", "Local Help", "Security Basics", "Web Lockout"]),
    ("Garden Projects", ["GPIO Choices", "External Relay Power", "Long Cable Runs", "Outdoor Enclosures", "Solenoid Valves", "Flow Measurement", "Build Zone One", "Add Zone Two", "Rain Skip Pattern", "Temperature Window Pattern", "Daily Health Report"]),
    ("Recovery and Release", ["Recover from Bad Automation", "Serial Rescue Workflow", "Safe Boot Thinking", "Firmware Upload", "Asset Upload", "Release Testing"]),
    ("Going Further", ["Command Reference Habits", "API Integrations", "Build a Morning Irrigation Program", "Build a Temperature Fan Program", "Build an Offline Logger", "Keep the System Fast", "Know the Limits", "Document Your Installation", "Final Beginner Checklist"]),
]


COMMAND_ATLAS = [
    ("Shell Essentials", ["help", "man", "clear", "echo", "history", "alias", "unalias", "env", "printenv", "set", "unset", "true", "false", "test", "repeat", "watch"], "These are the commands you use while learning and debugging. They make the board feel like a tiny UNIX shell."),
    ("System Information", ["version", "uname", "uptime", "free", "heap", "mem", "ps", "top", "pgrep", "pidof", "kill", "dmesg", "reboot", "resetreason", "chip", "flash", "sysinfo", "health", "diag"], "These commands answer the beginner question: is the board healthy, busy or running out of resources?"),
    ("Files and Storage", ["pwd", "cd", "ls", "cat", "head", "tail", "grep", "find", "wc", "du", "stat", "touch", "write", "append", "rm", "mkdir", "rmdir", "cp", "mv", "df", "fsformat"], "LittleFS is small, so inspect files and space regularly."),
    ("Network and Time", ["wifi scan", "wifi status", "wifi net", "wifi diag", "wifi connect", "wifi save", "wifi reconnect", "wifi wait", "wifi recover", "wifi sdkreset --yes", "ifconfig", "ip addr", "ap status", "hostname", "date", "ntp kick", "ntp sync"], "Networking and time are the foundation for web access, API integrations, logs and cron."),
    ("Automation and Hardware", ["relay add", "relay on", "relay off", "relay pulse", "timer once", "timer every", "cron add", "crontab -l", "rule add", "scene add", "state set", "let", "define", "function", "call", "input add", "when input", "sensor begin", "mail health"], "These are the commands that turn KernelESP from a tiny computer into a controller."),
]


def build() -> int:
    book = Book(OUT)
    book.cover()
    book.title_page()
    book.copyright_page()
    book.toc(PARTS)
    book.new_page("Learning Map")
    book.h1("The Learning Map", 0.95 * inch)
    book.p("Every chapter pair follows the same rhythm: first a plain-English explanation, then a lab page with commands, checks and a safe next step.")
    book.diagram_system_map(1.0 * inch, 4.9 * inch, 4.6 * inch, 2.0 * inch)
    book.y = 4.45 * inch
    book.bullet([
        "Inspect before changing: health, logs and status commands are your first tools.",
        "Prefer non-blocking actions: timers and cron are safer than long sleeps.",
        "Make names human: trees, flowers and fan are better than relay1, relay2 and gpio5.",
        "Keep recovery nearby: serial, backups and disarm are part of the design.",
    ])

    idx = 1
    topic_by_title = {t.title: t for t in TOPICS}
    for part_idx, (part_title, chapters) in enumerate(PARTS, start=1):
        diagram = ["system", "files", "wifi", "relay", "sensor", "automation", "web", "irrigation", "recovery", "api"][part_idx - 1]
        book.part_page(part_idx, part_title, f"This part covers {', '.join(chapters[:3])} and builds toward practical confidence.", diagram)
        for chapter in chapters:
            book.topic_pair(idx, topic_by_title[chapter])
            idx += 1

    book.part_page(11, "Command Atlas", "A compact appendix for commands you will use again and again.", "system")
    for title, commands, explanation in COMMAND_ATLAS:
        book.command_atlas_page(title, commands, explanation)

    book.new_page("Final Notes")
    book.h1("Final Notes", 0.95 * inch)
    book.p("KernelESP is deliberately small. That is its charm and its discipline. The board will reward careful habits: name things well, keep actions short, use timers, log important decisions, back up before experiments and keep a serial rescue path close.")
    book.callout("The beginner's rule", "If you can inspect it, explain it and undo it, you are ready to automate it.", PALETTE["soft_green"])
    book.code(["health", "free", "df", "crontab -l", "timer list", "input list", "backup"], "Before leaving a board unattended")
    book.save()
    return book.page


if __name__ == "__main__":
    pages = build()
    print(f"wrote {OUT.relative_to(ROOT)}")
    print(f"pages {pages}")
