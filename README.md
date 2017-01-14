# oftpd 0.3.7

## Installation
See the *INSTALL* file for directions on compiling and installing the binary.

Short version (as root):

```bash
 ./autogen.sh
 ./configure
 make
 make install
```

## Introduction
oftpd is designed to be as secure as an anonymous FTP server can possible be. 
It runs as non-root for most of the time, as uses the Unix `chroot(2)` command
to hide most of the systems directories from external users--they cannot change
into them even if the server is totally compromised! It contains its own
directory-change code, so that it can run efficiently as a threaded server, and its
own directory-listing code (many FTP servers execute the system as `ls` command
to list files). It is currently being code-reviewed for buffer overflows, and being load-tested.

## History
Shane Kerr, the original author, has this to say about the history of oftpd:

> I wrote oftpd to fill a need we had at my company. Our public FTP site
> was a mess, and in addition to reorganizing organizing [sic] the hierarchy and
> software. It turns out that the version we had had had [sic] a number of
> security issues. So I decided to find an anonymous-only, secure FTP
> server. None of the ones I found were fully baked. Time to write my own. :)

Unfortunately, oftpd has been abandoned since 2004. Like Shane, I have a need
for a secure, anonymous-only FTP server. So, without many options, I decided to
maintain it.

## Portability
This fork of oftpd is designed only for OpenBSD. Compatibility with Linux is not
guarenteed. See the *origin* branch or Shane Kerr's website for the original,
Linux-compatible oftpd.

## Credit
Original credit goes to:

> Shane Kerr

> shane@time-travellers.org
