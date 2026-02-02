/*
 * Nordic WiFi SoftAP Webserver - Main JavaScript
 */

// Configuration
const API_BASE = '';
const REFRESH_INTERVAL = 500; // ms

const BUTTON_PRESSED_COLOR = '#4caf50';
const BUTTON_RELEASED_COLOR = '#757575';

// State
let updateInterval = null;
let buttonGrid = null;
let buttonTemplate = null;
let buttonPlaceholder = null;
let ledGrid = null;
let ledTemplate = null;
let ledPlaceholder = null;

const buttonElements = new Map();
const ledElements = new Map();
let availableLedNumbers = [];

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    console.log('Nordic WiFi Controller initialized');

    buttonGrid = document.getElementById('button-grid');
    buttonTemplate = document.getElementById('button-template');
    buttonPlaceholder = document.getElementById('button-placeholder');
    ledGrid = document.getElementById('led-grid');
    ledTemplate = document.getElementById('led-template');
    ledPlaceholder = document.getElementById('led-placeholder');

    startAutoUpdate();
    updateButtonStates();
    updateLEDStates();
    
    // Set WiFi SSID (from CONFIG_APP_WIFI_SSID, hardcoded for now)
    document.getElementById('wifi-ssid').textContent = 'nRF70-WebServer';
});

// Start automatic updates
function startAutoUpdate() {
    if (updateInterval) {
        clearInterval(updateInterval);
    }
    
    updateInterval = setInterval(function() {
        updateButtonStates();
        updateLEDStates();
    }, REFRESH_INTERVAL);
    
    console.log('Auto-update started');
}

// Update button states from API
async function updateButtonStates() {
    try {
        const response = await fetch(`${API_BASE}/api/buttons`);
        if (!response.ok) {
            throw new Error(`HTTP error ${response.status}`);
        }
        
        const data = await response.json();
        
        if (data && Array.isArray(data.buttons)) {
            renderButtonStates(data.buttons);
        }
        
        // Update connection status
        updateConnectionStatus(true);
        
    } catch (error) {
        console.error('Failed to update button states:', error);
        if (buttonPlaceholder) {
            buttonPlaceholder.textContent = 'Failed to load button data';
            buttonPlaceholder.style.display = 'block';
        }
        updateConnectionStatus(false);
    }
}

// Update LED states from API
async function updateLEDStates() {
    try {
        const response = await fetch(`${API_BASE}/api/leds`);
        if (!response.ok) {
            throw new Error(`HTTP error ${response.status}`);
        }
        
        const data = await response.json();
        renderLedStates(data);
        
    } catch (error) {
        console.error('Failed to update LED states:', error);
        if (ledPlaceholder) {
            ledPlaceholder.textContent = 'Failed to load LED data';
            ledPlaceholder.style.display = 'block';
        }
    }
}

// Control LED
async function controlLED(ledNumber, action) {
    if (!availableLedNumbers.includes(ledNumber)) {
        console.warn(`LED ${ledNumber} is not available on this board.`);
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/api/led`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                led: ledNumber,
                action: action
            })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP error ${response.status}`);
        }
        
        console.log(`LED ${ledNumber} ${action} command sent`);
        
        // Immediately update LED state (optimistic update)
        setTimeout(() => updateLEDStates(), 100);
        
    } catch (error) {
        console.error('Failed to control LED:', error);
        alert(`Failed to control LED ${ledNumber}: ${error.message}`);
    }
}

// Update connection status indicator
function updateConnectionStatus(isConnected) {
    const statusElem = document.getElementById('connection-status');
    if (statusElem) {
        if (isConnected) {
            statusElem.innerHTML = '⬤ Connected';
            statusElem.className = 'info-value status-online';
        } else {
            statusElem.innerHTML = '⬤ Disconnected';
            statusElem.className = 'info-value status-offline';
        }
    }
}

// Cleanup on page unload
window.addEventListener('beforeunload', function() {
    if (updateInterval) {
        clearInterval(updateInterval);
    }
});

// Helpers
function renderButtonStates(buttons) {
    if (!buttonGrid || !buttonTemplate) {
        return;
    }

    const activeNumbers = new Set();

    if (buttonPlaceholder) {
        buttonPlaceholder.textContent = 'Waiting for button data...';
        buttonPlaceholder.style.display = buttons.length > 0 ? 'none' : 'block';
    }

    buttons.forEach(button => {
        const num = Number(button.number);
        if (!Number.isFinite(num) || num <= 0) {
            return;
        }

        activeNumbers.add(num);
        const entry = ensureButtonElement(num);
        if (!entry) {
            return;
        }

        const label = typeof button.name === 'string' && button.name.length > 0 ? button.name : `Button ${num}`;
        if (entry.nameElem) {
            entry.nameElem.textContent = label;
        }
        if (entry.stateElem) {
            entry.stateElem.textContent = button.pressed ? 'Pressed' : 'Released';
            entry.stateElem.style.color = button.pressed ? BUTTON_PRESSED_COLOR : BUTTON_RELEASED_COLOR;
        }
        if (entry.countElem) {
            entry.countElem.textContent = button.count || 0;
        }

        if (entry.container) {
            if (button.pressed) {
                entry.container.classList.add('active');
            } else {
                entry.container.classList.remove('active');
            }
        }
    });

    pruneInactiveButtons(activeNumbers);
}

function ensureButtonElement(number) {
    let entry = buttonElements.get(number);
    if (entry) {
        return entry;
    }

    const templateRoot = buttonTemplate.content.firstElementChild;
    if (!templateRoot) {
        return null;
    }

    const node = templateRoot.cloneNode(true);
    node.id = `button${number}`;

    const icon = node.querySelector('.button-icon');
    const nameElem = node.querySelector('.button-name');
    const stateElem = node.querySelector('.button-state');
    const countElem = node.querySelector('.button-count span');

    if (icon) {
        icon.textContent = number;
    }
    if (nameElem) {
        nameElem.textContent = `Button ${number}`;
    }
    if (stateElem) {
        stateElem.id = `btn${number}-state`;
    }
    if (countElem) {
        countElem.id = `btn${number}-count`;
    }

    buttonGrid.appendChild(node);

    entry = {
        container: node,
        nameElem,
        stateElem,
        countElem,
    };

    buttonElements.set(number, entry);
    return entry;
}

function pruneInactiveButtons(activeNumbers) {
    for (const [number, entry] of buttonElements.entries()) {
        if (!activeNumbers.has(number)) {
            entry.container.remove();
            buttonElements.delete(number);
        }
    }
}

function renderLedStates(data) {
    if (!ledGrid || !ledTemplate) {
        return;
    }

    const entries = Array.isArray(data?.leds) ? data.leds : [];

    const normalized = entries
        .map(item => ({
            number: Number(item.number),
            isOn: Boolean(item.is_on),
            name: typeof item.name === 'string' ? item.name : undefined,
        }))
        .filter(item => Number.isFinite(item.number) && item.number > 0)
        .sort((a, b) => a.number - b.number);

    if (ledPlaceholder) {
        ledPlaceholder.textContent = 'Waiting for LED data...';
        ledPlaceholder.style.display = normalized.length > 0 ? 'none' : 'block';
    }

    const activeNumbers = new Set();

    normalized.forEach(({ number, isOn, name }) => {
        activeNumbers.add(number);

        const entry = ensureLedElement(number);
        if (!entry) {
            return;
        }

        const label = name && name.length > 0 ? name : `LED ${number}`;
        if (entry.nameElem) {
            entry.nameElem.textContent = label;
        }
        if (entry.indicator) {
            if (isOn) {
                entry.indicator.classList.add('on');
            } else {
                entry.indicator.classList.remove('on');
            }
        }
    });

    pruneInactiveLeds(activeNumbers);
    availableLedNumbers = Array.from(activeNumbers).sort((a, b) => a - b);
}

function ensureLedElement(number) {
    let entry = ledElements.get(number);
    if (entry) {
        return entry;
    }

    const templateRoot = ledTemplate.content.firstElementChild;
    if (!templateRoot) {
        return null;
    }

    const node = templateRoot.cloneNode(true);
    node.id = `led${number}`;

    const indicator = node.querySelector('.led-indicator');
    const nameElem = node.querySelector('.led-name');
    const buttons = node.querySelectorAll('button[data-action]');

    if (indicator) {
        indicator.id = `led${number}-indicator`;
    }
    if (nameElem) {
        nameElem.textContent = `LED ${number}`;
    }

    buttons.forEach(button => {
        const action = button.dataset.action;
        if (!action) {
            return;
        }
        button.addEventListener('click', () => controlLED(number, action));
    });

    ledGrid.appendChild(node);

    entry = {
        container: node,
        indicator: indicator || node.querySelector('.led-indicator'),
        nameElem,
    };

    ledElements.set(number, entry);
    return entry;
}

function pruneInactiveLeds(activeNumbers) {
    for (const [number, entry] of ledElements.entries()) {
        if (!activeNumbers.has(number)) {
            entry.container.remove();
            ledElements.delete(number);
        }
    }
}
