# KernelESP Beginner Book

This directory contains the generated beginner-friendly PDF book:

```text
KernelESP_Beginners_Book.pdf
```

It is built from:

```sh
python tools/build-beginners-book.py
```

Install the local build dependencies with:

```sh
python -m pip install -r tools/requirements-book.txt
```

The book is generated with ReportLab and uses vector illustrations drawn by the
builder script, so it can be regenerated without external design tools. The PDF
also includes a clickable table of contents, internal bookmarks and a
Bauhaus-inspired visual system.
