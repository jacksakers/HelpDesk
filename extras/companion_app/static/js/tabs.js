// Project  : HelpDesk
// File     : js/tabs.js
// Purpose  : Tab switching logic — desktop top nav + mobile bottom nav
// Depends  : tasks.js, settings.js, drive.js, calendar.js

const TABS = ['status','calendar','tasks','media','drive','chat','settings'];

function showTab(name) {
    TABS.forEach(t => {
        document.getElementById('tab-' + t).classList.toggle('hidden', t !== name);

        // Desktop top nav button
        const btn = document.getElementById('tab-btn-' + t);
        if (btn) {
            btn.className = t === name
                ? 'py-2 px-3 rounded-lg text-sm font-medium bg-gray-700 text-white transition whitespace-nowrap'
                : 'py-2 px-3 rounded-lg text-sm font-medium text-gray-400 hover:text-white transition whitespace-nowrap';
        }

        // Mobile bottom nav button
        const mob = document.getElementById('mob-btn-' + t);
        if (mob) {
            mob.className = 'flex-1 flex flex-col items-center justify-center py-1.5 transition ' +
                (t === name ? 'text-purple-400' : 'text-gray-500 hover:text-gray-300');
        }
    });

    if (name === 'settings') loadSettings();
    if (name === 'tasks')    loadTasks();
    if (name === 'drive')    driveLoad();
    if (name === 'calendar') loadCalendar();
}
