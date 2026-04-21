// Project  : HelpDesk
// File     : js/calendar.js
// Purpose  : Calendar tab — month view, day events, add/delete events

let _calEvents       = [];
let _calYear         = new Date().getFullYear();
let _calMonth        = new Date().getMonth();   // 0-indexed
let _calSelectedDate = '';

const _MONTH_NAMES = [
    'January','February','March','April','May','June',
    'July','August','September','October','November','December'
];

async function loadCalendar() {
    try {
        const resp = await fetch('/api/calendar');
        const data = await resp.json();
        _calEvents = Array.isArray(data) ? data : [];
    } catch (e) {
        _calEvents = [];
    }
    renderCalendarMonth(_calYear, _calMonth);
    if (_calSelectedDate) renderCalendarDayEvents(_calSelectedDate);
}

function renderCalendarMonth(year, month) {
    _calYear  = year;
    _calMonth = month;
    document.getElementById('cal-month-label').textContent =
        _MONTH_NAMES[month] + ' ' + year;

    const grid = document.getElementById('cal-grid');
    grid.innerHTML = '';

    const today       = new Date();
    const todayStr    = _dateStr(today.getFullYear(), today.getMonth() + 1, today.getDate());
    const firstDay    = new Date(year, month, 1).getDay();              // 0=Sun
    const mondayStart = (firstDay === 0) ? 6 : firstDay - 1;           // 0=Mon
    const daysInMonth = new Date(year, month + 1, 0).getDate();

    const eventDates = new Set(_calEvents.map(e => e.date));

    // Empty cells before the 1st
    for (let i = 0; i < mondayStart; i++) {
        const empty = document.createElement('div');
        empty.className = 'h-9';
        grid.appendChild(empty);
    }

    for (let d = 1; d <= daysInMonth; d++) {
        const dateStr  = _dateStr(year, month + 1, d);
        const hasEvent = eventDates.has(dateStr);
        const isToday  = dateStr === todayStr;
        const isSel    = dateStr === _calSelectedDate;

        const cell = document.createElement('button');
        cell.onclick = () => calSelectDay(dateStr);
        cell.className = 'relative h-9 w-full rounded-lg text-sm font-medium transition ' + (
            isSel   ? 'bg-purple-700 text-white' :
            isToday ? 'bg-purple-900 text-purple-200 ring-1 ring-purple-500' :
                      'hover:bg-gray-700 text-gray-300'
        );
        cell.textContent = d;

        if (hasEvent) {
            const dot = document.createElement('span');
            dot.className = 'absolute bottom-1 left-1/2 -translate-x-1/2 w-1 h-1 rounded-full bg-purple-400 pointer-events-none';
            cell.appendChild(dot);
        }
        grid.appendChild(cell);
    }
}

function calSelectDay(dateStr) {
    _calSelectedDate = dateStr;
    renderCalendarMonth(_calYear, _calMonth);
    renderCalendarDayEvents(dateStr);
    // Pre-fill date in modal
    document.getElementById('evt-date').value = dateStr;
}

function renderCalendarDayEvents(dateStr) {
    const [y, m, d] = dateStr.split('-').map(Number);
    document.getElementById('cal-day-title').textContent =
        _MONTH_NAMES[m - 1] + ' ' + d + ', ' + y;

    const container = document.getElementById('cal-day-events');
    container.innerHTML = '';

    const dayEvents = _calEvents.filter(e => e.date === dateStr);
    if (dayEvents.length === 0) {
        container.innerHTML =
            '<p class="text-gray-500 text-sm italic">No events. Use + Add Event to create one.</p>';
        return;
    }

    dayEvents.forEach(e => {
        const timeStr = e.all_day
            ? 'All day'
            : (e.start_time || '--') + (e.end_time ? '\u2013' + e.end_time : '');

        const row = document.createElement('div');
        row.className = 'flex items-center gap-3 bg-gray-700 rounded-lg px-3 py-2';

        const time = document.createElement('span');
        time.className = 'text-xs text-purple-300 font-mono w-24 shrink-0';
        time.textContent = timeStr;

        const title = document.createElement('span');
        title.className = 'flex-1 text-sm text-white truncate';
        title.textContent = e.title;

        const del = document.createElement('button');
        del.textContent = '\u00d7';
        del.className = 'text-gray-500 hover:text-red-400 text-lg leading-none transition shrink-0';
        del.onclick = () => deleteCalendarEvent(e.id, dateStr);

        row.appendChild(time);
        row.appendChild(title);
        row.appendChild(del);
        container.appendChild(row);
    });
}

function calPrevMonth() {
    _calMonth--;
    if (_calMonth < 0) { _calMonth = 11; _calYear--; }
    renderCalendarMonth(_calYear, _calMonth);
}

function calNextMonth() {
    _calMonth++;
    if (_calMonth > 11) { _calMonth = 0; _calYear++; }
    renderCalendarMonth(_calYear, _calMonth);
}

function showAddEventModal() {
    if (_calSelectedDate) {
        document.getElementById('evt-date').value = _calSelectedDate;
    }
    document.getElementById('evt-title').value        = '';
    document.getElementById('evt-start').value        = '';
    document.getElementById('evt-end').value          = '';
    document.getElementById('evt-allday').checked     = false;
    document.getElementById('evt-start').disabled     = false;
    document.getElementById('evt-end').disabled       = false;
    document.getElementById('add-evt-status').classList.add('hidden');
    document.getElementById('add-event-modal').classList.remove('hidden');
    document.getElementById('evt-title').focus();
}

function closeAddEventModal() {
    document.getElementById('add-event-modal').classList.add('hidden');
}

function toggleAllDay(cb) {
    document.getElementById('evt-start').disabled = cb.checked;
    document.getElementById('evt-end').disabled   = cb.checked;
}

async function submitAddEvent() {
    const title   = document.getElementById('evt-title').value.trim();
    const date    = document.getElementById('evt-date').value;
    const start   = document.getElementById('evt-start').value;
    const end     = document.getElementById('evt-end').value;
    const all_day = document.getElementById('evt-allday').checked;
    const statusEl = document.getElementById('add-evt-status');

    if (!title) {
        statusEl.textContent = 'Title is required.';
        statusEl.className   = 'text-xs mt-2 text-red-400';
        statusEl.classList.remove('hidden');
        return;
    }
    if (!date) {
        statusEl.textContent = 'Date is required.';
        statusEl.className   = 'text-xs mt-2 text-red-400';
        statusEl.classList.remove('hidden');
        return;
    }

    statusEl.textContent = 'Saving\u2026';
    statusEl.className   = 'text-xs mt-2 text-gray-400';
    statusEl.classList.remove('hidden');

    try {
        const res = await fetch('/api/calendar/add', {
            method:  'POST',
            headers: { 'Content-Type': 'application/json' },
            body:    JSON.stringify({ title, date, start_time: start, end_time: end, all_day }),
        });
        if (res.ok) {
            closeAddEventModal();
            await loadCalendar();
            if (_calSelectedDate) renderCalendarDayEvents(_calSelectedDate);
        } else {
            const err = await res.json().catch(() => ({}));
            statusEl.textContent = 'Error: ' + (err.error || err.detail || res.status);
            statusEl.className   = 'text-xs mt-2 text-red-400';
        }
    } catch (e) {
        statusEl.textContent = 'Could not reach device.';
        statusEl.className   = 'text-xs mt-2 text-red-400';
    }
}

async function deleteCalendarEvent(id, dateStr) {
    if (!confirm('Delete this event?')) return;
    try {
        const res = await fetch('/api/calendar/delete', {
            method:  'POST',
            headers: { 'Content-Type': 'application/json' },
            body:    JSON.stringify({ id }),
        });
        if (res.ok) {
            await loadCalendar();
            if (dateStr) renderCalendarDayEvents(dateStr);
        }
    } catch (e) {
        alert('Could not reach device.');
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

function _dateStr(y, m, d) {
    return `${y}-${String(m).padStart(2,'0')}-${String(d).padStart(2,'0')}`;
}
