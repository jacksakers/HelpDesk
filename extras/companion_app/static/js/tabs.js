// Project  : HelpDesk
// File     : js/tabs.js
// Purpose  : Tab switching logic
// Depends  : tasks.js, settings.js, drive.js

const TABS = ['status','tasks','media','drive','chat','settings'];

function showTab(name) {
    TABS.forEach(t => {
        document.getElementById('tab-'+t).classList.toggle('hidden', t !== name);
        const btn = document.getElementById('tab-btn-'+t);
        btn.className = t === name
            ? 'flex-1 py-2 px-4 rounded-lg text-sm font-medium bg-gray-700 text-white transition'
            : 'flex-1 py-2 px-4 rounded-lg text-sm font-medium text-gray-400 hover:text-white transition';
    });
    if (name === 'settings') loadSettings();
    if (name === 'tasks')    loadTasks();
    if (name === 'drive')    driveLoad();
}
