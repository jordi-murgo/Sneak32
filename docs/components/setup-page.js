import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakSetup extends LitElement {
    static get properties() {
        return {
            settings: { type: Object },
            isConnected: { type: Boolean }
        };
    }

    constructor() {
        super();
        // Initialize settings with default values
        this.settings = {
            deviceName: '',
            operationMode: 0,
            cpuSpeed: 80,
            minimalRSSI: -60,
            autosaveInterval: 60,
            ledMode: 1,
            onlyManagementFrames: false,
            passiveScan: false,
            stealthMode: false,
            authorizedMAC: '',
            wifiTxPower: 20,
            dwellTime: 10000,
            bleScanPeriod: 30,
            bleScanDuration: 15,
            ignoreRandomBLE: false,
            bleTxPower: 0,
            bleMTU: 256,
            ignoreLocalWifiAddresses: false
        };
        this.isLoading = false;

        // Listen for loading events
        document.addEventListener('device-info-loading', () => {
            console.log('🥁 DeviceInfo :: Device info loading');
            this.isLoading = true;
        });

        document.addEventListener('device-info-loaded', (event) => {
            console.log('🥁 DeviceInfo :: Device info loaded');
            this.isLoading = false;
            this.handleDeviceSettingsLoad(event.detail.settings);
        });

    }

    // Method to update settings
    updateSetting(key, value) {
        this.settings = {
            ...this.settings,
            [key]: value
        };
        this.requestUpdate();
    }

    // Method to handle loading device settings
    handleDeviceSettingsLoad(settings) {
        this.settings = {
            ...this.settings,
            ...settings
        };
        this.requestUpdate();
    }

    // Method to save settings to the device
    saveSettings() {
        console.log('Saving settings:', this.settings);

        // Dispatch an event with the updated settings
        this.dispatchEvent(new CustomEvent('settings-updated', {
            detail: this.settings,
            bubbles: true,
            composed: true
        }));
    }

    render() {
        return html`
            <div id="setup-content">

                <ion-list>
                    <!-- General Configuration -->
                    <ion-item-group>
                        <ion-item-divider color="medium">
                            <ion-icon name="settings-outline" slot="start"></ion-icon>
                            <ion-label>General Configuration</ion-label>
                        </ion-item-divider>

                        <!-- Device Name -->
                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Device Name</h2>
                                <p>Set a unique name to identify this device</p>
                            </ion-label>
                            <ion-input 
                                fill="solid"
                                type="text"
                                .value=${this.settings.deviceName}
                                @ionChange=${(e) => this.updateSetting('deviceName', e.detail.value)}
                            ></ion-input>
                        </ion-item>

                        <!-- Operation Mode -->
                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Operation Mode</h2>
                                <p>Select how the device will operate and process data</p>
                            </ion-label>
                            <ion-select
                                fill="solid"
                                .value=${this.settings.operationMode}
                                @ionChange=${(e) => this.updateSetting('operationMode', e.detail.value)}
                            >
                                <ion-select-option .value=${0}>OFF</ion-select-option>
                                <ion-select-option .value=${1}>Capture (RECON)</ion-select-option>
                                <ion-select-option .value=${2}>Detection (DEFENSE)</ion-select-option>
                            </ion-select>
                        </ion-item>

                        <!-- CPU Speed -->
                        <ion-item>
                            <ion-label position="stacked">
                                <h2>CPU Speed</h2>
                                <p>Higher speed increases performance but consumes more power</p>
                            </ion-label>
                            <ion-select
                                fill="solid"
                                .value=${this.settings.cpuSpeed}
                                @ionChange=${(e) => this.updateSetting('cpuSpeed', e.detail.value)}
                            >
                                <ion-select-option .value=${80}>80 MHz (Low Power)</ion-select-option>
                                <ion-select-option .value=${160}>160 MHz (Normal)</ion-select-option>
                            </ion-select>
                        </ion-item>

                        <!-- Minimal RSSI -->
                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Minimal RSSI</h2>
                                <p>Filter out devices with signal strength below this threshold</p>
                            </ion-label>
                            <ion-select
                                fill="solid"
                                .value=${this.settings.minimalRSSI}
                                @ionChange=${(e) => this.updateSetting('minimalRSSI', e.detail.value)}
                            >
                                <ion-select-option .value=${-30}>-30 dBm (Very close)</ion-select-option>
                                <ion-select-option .value=${-35}>-35 dBm</ion-select-option>
                                <ion-select-option .value=${-40}>-40 dBm</ion-select-option>
                                <ion-select-option .value=${-45}>-45 dBm</ion-select-option>
                                <ion-select-option .value=${-50}>-50 dBm</ion-select-option>
                                <ion-select-option .value=${-55}>-55 dBm</ion-select-option>
                                <ion-select-option .value=${-60}>-60 dBm (Medium)</ion-select-option>
                                <ion-select-option .value=${-65}>-65 dBm</ion-select-option>
                                <ion-select-option .value=${-70}>-70 dBm</ion-select-option>
                                <ion-select-option .value=${-75}>-75 dBm</ion-select-option>
                                <ion-select-option .value=${-80}>-80 dBm</ion-select-option>
                                <ion-select-option .value=${-85}>-85 dBm (Far)</ion-select-option>
                                <ion-select-option .value=${-90}>-90 dBm</ion-select-option>
                                <ion-select-option .value=${-95}>-95 dBm</ion-select-option>
                                <ion-select-option .value=${-100}>-100 dBm (Very far)</ion-select-option>
                            </ion-select>
                        </ion-item>

                        <!-- Autosave Interval -->
                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Autosave Interval (minutes)</h2>
                                <p>Set the interval for automatic saving of captured data</p>
                            </ion-label>
                            <ion-input
                                fill="solid"
                                type="number"
                                .value=${this.settings.autosaveInterval}
                                @ionChange=${(e) => this.updateSetting('autosaveInterval', parseInt(e.detail.value))}
                            ></ion-input>
                        </ion-item>

                        <!-- LED Mode -->
                        <ion-item>
                            <ion-label position="stacked">
                                <h2>LED Signaling</h2>
                                <p>Enable LED signaling to indicate activity or status changes</p>
                            </ion-label>
                            <ion-toggle
                                fill="solid"
                                slot="end"
                                .checked=${this.settings.ledMode === 1}
                                @ionChange=${(e) => this.updateSetting('ledMode', e.detail.checked ? 1 : 0)}
                            />
                        </ion-item>
                    </ion-item-group>

                    <!-- Security Configuration -->
                    <ion-item-group>
                        <ion-item-divider color="medium">
                            <ion-icon name="shield-half" slot="start"></ion-icon>
                            <ion-label>Security</ion-label>
                        </ion-item-divider>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Passive Scan</h2>
                                <p>Only listen for signals without transmitting probes</p>
                            </ion-label>
                            <ion-toggle
                                fill="solid"
                                slot="end"
                                .checked=${this.settings.passiveScan}
                                @ionChange=${(e) => this.updateSetting('passiveScan', e.detail.checked)}
                            />
                        </ion-item>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Stealth Mode</h2>
                                <p>Minimize device visibility and radio emissions</p>
                            </ion-label>
                            <ion-toggle
                                fill="solid"
                                slot="end"
                                .checked=${this.settings.stealthMode}
                                @ionChange=${(e) => this.updateSetting('stealthMode', e.detail.checked)}
                            />
                        </ion-item>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Authorized MAC</h2>
                                <p>MAC address allowed to connect to this device when in stealth capture mode</p>
                            </ion-label>
                            <ion-input
                                fill="solid"
                                type="text"
                                .value=${this.settings.authorizedMAC}
                                @ionChange=${(e) => this.updateSetting('authorizedMAC', e.detail.value)}
                            />
                        </ion-item>
                    </ion-item-group>

                    <!-- WiFi Configuration -->
                    <ion-item-group>
                        <ion-item-divider color="medium">
                            <ion-icon name="wifi" slot="start"></ion-icon>
                            <ion-label>WiFi Configuration</ion-label>
                        </ion-item-divider>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Only Management Frames</h2>
                                <p>Capture only WiFi management frames</p>
                            </ion-label>
                            <ion-toggle
                                fill="solid"
                                slot="end"
                                .checked=${this.settings.onlyManagementFrames}
                                @ionChange=${(e) => this.updateSetting('onlyManagementFrames', e.detail.checked)}
                            />
                        </ion-item>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Ignore Local WiFi Addresses</h2>
                                <p>Filter out locally administered MAC addresses</p>
                            </ion-label>
                            <ion-toggle
                                fill="solid"
                                slot="end"
                                .checked=${this.settings.ignoreLocalWifiAddresses}
                                @ionChange=${(e) => this.updateSetting('ignoreLocalWifiAddresses', e.detail.checked)}
                            />
                        </ion-item>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Dwell Channel Time</h2>
                                <p>Time spent scanning each WiFi channel in milliseconds</p>
                            </ion-label>
                            <ion-input
                                fill="solid"
                                type="number"
                                .value=${this.settings.dwellTime}
                                @ionChange=${(e) => this.updateSetting('dwellTime', parseInt(e.detail.value, 10))}
                            />
                        </ion-item>
                    </ion-item-group>

                    

                    <!-- BLE Configuration -->
                    <ion-item-group>
                        <ion-item-divider color="medium">
                            <ion-icon name="bluetooth" slot="start"></ion-icon>
                            <ion-label>BLE Configuration</ion-label>
                        </ion-item-divider>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>BLE Scan Period</h2>
                                <p>Interval between BLE scans in seconds</p>
                            </ion-label>
                            <ion-input
                                fill="solid"
                                type="number"
                                .value=${this.settings.bleScanPeriod}
                                @ionChange=${(e) => this.updateSetting('bleScanPeriod', parseInt(e.detail.value, 10))}
                            />
                        </ion-item>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>BLE Scan Duration</h2>
                                <p>How long each BLE scan lasts in seconds</p>
                            </ion-label>
                            <ion-input
                                fill="solid"
                                type="number"
                                .value=${this.settings.bleScanDuration}
                                @ionChange=${(e) => this.updateSetting('bleScanDuration', parseInt(e.detail.value, 10))}
                            />
                        </ion-item>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>Ignore Random BLE</h2>
                                <p>Filter out devices using random MAC addresses</p>
                            </ion-label>
                            <ion-toggle
                                slot="end"
                                .checked=${this.settings.ignoreRandomBLE}
                                @ionChange=${(e) => this.updateSetting('ignoreRandomBLE', e.detail.checked)}
                            />
                        </ion-item>

                        <ion-item>
                            <ion-label position="stacked">
                                <h2>BLE MTU Size</h2>
                                <p>Set the Maximum Transmission Unit for Bluetooth communication (32-512 bytes)</p>
                            </ion-label>
                            <ion-input
                                type="number"
                                fill="solid"
                                .value=${this.settings.bleMTU}
                                @ionChange=${(e) => {
                const value = parseInt(e.detail.value, 10);
                // Validate input range
                if (value >= 32 && value <= 1024) {
                    this.updateSetting('bleMTU', value);
                }
            }}
                                min={32}
                                max={1024}
                            />
                        </ion-item>
                    </ion-item-group>

                    <!-- Save Button -->
                    <div class="save-button-container">
                        <ion-button 
                            @click=${this.saveSettings}
                            color="primary"
                            size="large"
                            expand="block"
                        >
                            <ion-icon name="save-outline" slot="start"></ion-icon>
                            Save Settings
                        </ion-button>
                    </div>
                </ion-list>
            </div>
        `;
    }

    static styles = css`
        :host {
            display: block;
            padding: 10px 0;
        }

        ion-list {
            padding-top: 0px !important;
            padding-bottom: 0px !important;
        }

        .save-button-container {
            padding: 16px;
            margin-top: 16px;
        }

        /* Estilos del tema global */
        ion-input, ion-select {
            margin-top: 0.25rem;
        }

        .divider-label {
            font-size: 1.2rem !important;
            font-weight: 500 !important;
            color: var(--ion-color-light) !important;
            letter-spacing: 0.05rem;
        }

        ion-label[position="stacked"] {
            margin-bottom: 1em !important;
        }

        .setting-item {
            margin-bottom: 1.5rem;
        }

        ion-content {
            --overflow: scroll;
            --padding-bottom: 20px;
        }

        ion-content::part(scroll) {
            overflow-y: auto !important;
        }

        .save-button-container ion-button {
            background: var(--ion-color-primary);
            color: var(--ion-color-primary-contrast);
        }

        .save-button-container ion-button:hover {
            background: var(--ion-color-primary-shade);
        }
    
        ion-item-divider {
            background: var(--ion-color-medium);
            color: var(--ion-color-light);
        }

        ion-item-divider>ion-label {
            font-size: 1.2rem !important;
            font-weight: 500 !important;
            color: var(--ion-color-light) !important;
            letter-spacing: 0.05rem;
        }

        ion-item-group {
            margin-bottom: 16px !important;
        }

        .sc-ion-label-md-s h2 {
            margin-left: 0;
            margin-right: 0;
            margin-top: 2px;
            margin-bottom: 2px;
            font-size: 1.3rem;
            font-weight: normal;
        }
    `;
}

customElements.define('sneak-setup', SneakSetup);