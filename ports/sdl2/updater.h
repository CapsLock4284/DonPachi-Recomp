/**
 *    Name:        updater.h
 *    Description: the program updating itself: the newest release looked up on GitHub as the program starts, and on
 *                 the player's yes downloaded, put in place of the program's own files and started; the player's
 *                 settings, scores, replays and the game's data are never touched
 *    Version:     0.1.0
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    The same file in DonPachi Recomp and DoDonPachi Recomp (each port's own repository and package name given to
 *    updater_check). The release: GitHub's newest release of the repository (its API's releases/latest: the tag,
 *    "v0.3.0" or "0.3.0", and its assets), its asset whose name ends in the package's suffix ("-win64.zip"), the
 *    package tools/make_release.ps1 makes: the program's own files only, in one folder.
 *    Installing: the .zip downloaded, its files unpacked into update_new/ beside the program, then each put in place
 *    of the one it replaces (that one renamed to NAME.old first: Windows lets a running program's file be renamed,
 *    not written), the names of the .old files kept in update_old.txt, and, once the program has saved its files and
 *    closed, the new program started with the same arguments; the next start deletes the .old files (updater_cleanup). The download: WinHTTP on Windows; elsewhere
 *    the curl command (a Linux build updates itself only when it is run from its own folder, not an AppImage).
 *    A check, a download and the unpacking run on a thread of their own: the game goes on meanwhile.
 */
#ifndef UPDATER_H
#define UPDATER_H

#include <stdbool.h>

enum
{
	UPDATER_IDLE,               /* not looked yet (or the check is off) */
	UPDATER_CHECKING,
	UPDATER_UP_TO_DATE,
	UPDATER_AVAILABLE,          /* a newer release: updater_latest() */
	UPDATER_DOWNLOADING,        /* updater_progress() */
	UPDATER_READY,              /* downloaded and unpacked: updater_install */
	UPDATER_FAILED              /* updater_problem() */
};

/* As the program starts, beside it ('folder', ending in its separator): the files an update before left (.old) */
void updater_cleanup(const char *folder);

/* The newest release of 'repository' ("owner/name") looked up, on a thread: newer than 'version' ("0.2.0"), with an
   asset ending in 'suffix', it is AVAILABLE */
void updater_check(const char *folder, const char *repository, const char *suffix, const char *version);

int updater_state(void);
const char *updater_latest(void);       /* the newer release's version ("0.3.0") */
int updater_progress(void);             /* the download, 0-100 */
const char *updater_problem(void);

/* The newer release downloaded and unpacked, on a thread (AVAILABLE, or FAILED: tried again) */
void updater_download(void);

/* READY: the new files put in place (the ones they replace renamed .old, deleted at the next start); false:
   nothing changed (or all was put back), updater_problem() says why */
bool updater_install(void);

/* Installed: the new program started with the program's 'argv' (on Linux in place of this one: it does not return),
   after the program has saved its files and closed its window; false: it could not be (the player starts it) */
bool updater_restart(char **argv);

#endif
