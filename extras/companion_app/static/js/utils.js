// Project  : HelpDesk
// File     : js/utils.js
// Purpose  : Shared utility helpers used across all modules

function escapeHtml(s) {
    return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

// Alias — chat.js uses the shorter name
const escHtml = escapeHtml;
