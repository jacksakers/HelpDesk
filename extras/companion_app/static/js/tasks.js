// Project  : HelpDesk
// File     : js/tasks.js
// Purpose  : Tasks tab — list, add, complete, and delete tasks

let _taskData = [];

async function loadTasks() {
    try {
        const data = await (await fetch('/api/tasks')).json();
        _taskData = data.tasks || [];
        renderTasks(_taskData, data);
    } catch(e) {
        document.getElementById('task-list').innerHTML =
            '<p class="text-red-400 text-sm">Could not reach device. Check Settings \u2192 HelpDesk IP.</p>';
    }
}

function renderTasks(tasks, stats) {
    const goal = 50;
    const pct  = Math.min(100, Math.round((stats.daily_xp || 0) / goal * 100));
    document.getElementById('task-level').textContent     = stats.level ?? '--';
    document.getElementById('task-total-xp').textContent  = (stats.total_xp ?? '--') + ' XP';
    document.getElementById('task-streak').textContent    = (stats.streak ?? '--') + 'd';
    document.getElementById('task-daily-xp-val').textContent = (stats.daily_xp || 0) + ' / ' + goal;
    document.getElementById('task-xp-bar').style.width   = pct + '%';
    document.getElementById('task-xp-bar').className     =
        'h-3 rounded-full transition-all duration-500 ' +
        (pct >= 100 ? 'bg-yellow-400' : 'bg-emerald-500');

    const list = document.getElementById('task-list');
    const empty = document.getElementById('task-list-empty');
    if (empty) empty.remove();
    list.innerHTML = '';

    if (!tasks || tasks.length === 0) {
        list.innerHTML = '<p class="text-gray-500 text-sm italic">No tasks yet.</p>';
        return;
    }

    const active = tasks.filter(t => !t.done);
    const done   = tasks.filter(t =>  t.done);

    [...active, ...done].forEach(t => {
        const row = document.createElement('div');
        row.className = 'flex items-center gap-3 p-2 rounded-lg ' +
            (t.done ? 'bg-gray-900 opacity-60' : 'bg-gray-700');

        const icon = document.createElement('span');
        icon.textContent = t.done ? '\u2713' : '\u25cb';
        icon.className = t.done ? 'text-emerald-400 w-5 text-center shrink-0'
                                 : 'text-gray-400 w-5 text-center shrink-0 cursor-pointer';
        if (!t.done) icon.onclick = () => completeTask(t.id);

        const text = document.createElement('span');
        text.textContent = t.text;
        text.className = 'flex-1 text-sm ' + (t.done ? 'line-through text-gray-500' : 'text-white');

        const badges = document.createElement('div');
        badges.className = 'flex items-center gap-2';
        if (t.due_date) {
            const due = document.createElement('span');
            due.textContent = t.due_date;
            due.title = 'Due date';
            due.className = 'text-xs px-1.5 py-0.5 rounded bg-orange-900 text-orange-300 font-mono';
            badges.appendChild(due);
        }
        if (t.repeat) {
            const rep = document.createElement('span');
            rep.textContent = '\u21ba';
            rep.title = 'Repeating daily';
            rep.className = 'text-blue-400 text-xs';
            badges.appendChild(rep);
        }
        const xp = document.createElement('span');
        xp.textContent = '+10 XP';
        xp.className = t.done ? 'text-gray-600 text-xs' : 'text-yellow-500 text-xs font-mono';
        badges.appendChild(xp);

        const del = document.createElement('button');
        del.textContent = '\u00d7';
        del.className = 'text-gray-500 hover:text-red-400 text-sm transition ml-1';
        del.onclick = () => deleteTask(t.id);
        badges.appendChild(del);

        row.appendChild(icon);
        row.appendChild(text);
        row.appendChild(badges);
        list.appendChild(row);
    });
}

async function addTask() {
    const input    = document.getElementById('task-new-text');
    const repeat   = document.getElementById('task-repeat').checked;
    const dueDateEl = document.getElementById('task-due-date');
    const due_date  = dueDateEl ? dueDateEl.value : '';
    const text      = input.value.trim();
    if (!text) return;
    const statusEl = document.getElementById('task-add-status');
    statusEl.textContent = 'Adding...';
    statusEl.className = 'mt-2 text-xs text-gray-400';
    statusEl.classList.remove('hidden');
    try {
        const payload = {text, repeat};
        if (due_date) payload.due_date = due_date;
        const res = await fetch('/api/tasks/add', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload)
        });
        if (res.ok) {
            input.value = '';
            document.getElementById('task-repeat').checked = false;
            if (dueDateEl) dueDateEl.value = '';
            statusEl.textContent = 'Added!';
            statusEl.className = 'mt-2 text-xs text-emerald-400';
            loadTasks();
        } else {
            const d = await res.json();
            statusEl.textContent = 'Error: ' + (d.detail || d.error || 'Unknown');
            statusEl.className = 'mt-2 text-xs text-red-400';
        }
    } catch(e) {
        statusEl.textContent = 'Network error';
        statusEl.className = 'mt-2 text-xs text-red-400';
    }
    setTimeout(() => statusEl.classList.add('hidden'), 3000);
}

async function completeTask(id) {
    try {
        await fetch('/api/tasks/complete?task_id=' + id, {method: 'POST'});
        loadTasks();
    } catch {}
}

async function deleteTask(id) {
    try {
        await fetch('/api/tasks/delete?task_id=' + id, {method: 'POST'});
        loadTasks();
    } catch {}
}
