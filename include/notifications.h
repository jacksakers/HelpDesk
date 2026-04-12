// Project  : HelpDesk
// File     : notifications.h
// Purpose  : Notification store — receives companion serial events, groups by sender,
//            flashes the launcher tile, and drives the Notifications screen list
// Depends  : ui_Screen1.h (for ui_NotifTile / ui_NotifBadge)
//            ui_Screen4.h (for ui_NotifList)

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Call once in setup() after Serial.begin().
void notifInit(void);

// Called by pc_monitor when a {"cmd":"notif",...} packet arrives.
// app   : originating app name (e.g. "Slack")
// title : sender name / notification title (e.g. "Alice Smith")
// body  : message text (already truncated to 50 chars by companion)
void notifAdd(const char * app, const char * title, const char * body);

// Returns current number of unread notification groups.
int notifGetUnreadCount(void);

// Returns total grouped-sender count (read + unread).
int notifGetGroupCount(void);

// Clears the unread counter and updates the launcher badge.
// Called by ui_Screen4 on open so the badge disappears.
void notifMarkAllRead(void);

// Clears all stored notifications and refreshes the screen list.
// Called by the "Clear All" button on Screen4.
void notifClearAll(void);

// Rebuilds all rows inside ui_NotifList (if Screen4 is currently loaded).
// Called automatically by notifAdd() and notifClearAll().
void notifRefreshScreen(void);

// Called every loop() iteration.
// Drives the launcher tile flash animation when there are unread notifications.
void handleNotifications(unsigned long now_ms);

#ifdef __cplusplus
} /* extern "C" */
#endif
