import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakDeviceInfo extends LitElement {
    static properties = {
        operationMode: {
            type: String,
            attribute: 'operation-mode'
        },
        stats: {
            type: Object
        },
        firmwareInfo: {
            type: Object,
            attribute: 'firmware-info'
        },
        deviceStatus: {
            type: Object
        },
        isLoading: { type: Boolean }
    };

    constructor() {
        super();
        this.operationMode = 'Off mode';
        this.deviceName = 'Unknown Device';
        this.deviceStatus = {
            wifiNetworks: 0,
            wifiDevices: 0,
            bleDevices: 0,
            wifiDetectedNetworks: 0,
            wifiDetectedDevices: 0,
            bleDetectedDevices: 0,
            alarm: false,
            freeHeap: 'N/A',
            uptime: Date.now()
        };
        this.firmwareInfo = {
            version: 'N/A',
            chip: 'N/A',
            board: 'N/A',
            cpu_mhz: 'N/A',
            flash_kb: 'N/A',
            flash_mhz: 'N/A',
            ram_kb: 'N/A',
            bt_mac: 'N/A',
            ap_mac: 'N/A',
            arch: 'N/A',
            pio_ver: 'N/A',
            ard_ver: 'N/A',
            gcc_ver: 'N/A',
            built: 0,
            size_kb: 'N/A',
            md5: 'N/A'
        };
        this.settings = {
            operationMode: 0,
            minimalRSSI: -100
        };
        this.localPowerOnTime = Date.now() / 1000;
        this.localUptime = 0;
        this.localUptimeInterval = null;
        this.isLoading = false;
        this.isWifiNetworksLoading = false;
        this.isWifiDevicesLoading = false;
        this.isBleDevicesLoading = false;

        // Listen for loading events
        document.addEventListener('device-info-loading', () => {
            console.log('🥁 DeviceInfo :: Device info loading');
            this.isLoading = true;
        });

        document.addEventListener('device-info-loaded', (event) => {
            console.log('🥁 DeviceInfo :: Device info loaded');
            this.isLoading = false;
            this.updateDeviceInfo(event.detail);
        });

        document.addEventListener('wifi-networks-loading', () => {
            console.log('🥁 DeviceInfo :: WiFi networks loading');
            this.isWifiNetworksLoading = true;
            this.requestUpdate();
        });

        document.addEventListener('wifi-networks-error', (event) => {
            console.log('🥁 DeviceInfo :: WiFi networks loading error');
            this.isWifiNetworksLoading = false;
            this.requestUpdate();
        });

        document.addEventListener('wifi-networks-loaded', (event) => {
            console.log('🥁 DeviceInfo :: WiFi networks loaded');
            this.isWifiNetworksLoading = false;
            this.requestUpdate();
        });

        document.addEventListener('wifi-devices-loading', () => {
            console.log('🥁 DeviceInfo :: WiFi devices loading');
            this.isWifiDevicesLoading = true;
            this.requestUpdate();
        });

        document.addEventListener('wifi-devices-error', (event) => {
            console.log('🥁 DeviceInfo :: WiFi devices loading error');
            this.isWifiDevicesLoading = false;
            this.requestUpdate();
        });

        document.addEventListener('wifi-devices-loaded', (event) => {
            console.log('🥁 DeviceInfo :: WiFi devices loaded');
            this.isWifiDevicesLoading = false;
            this.requestUpdate();
        });


        document.addEventListener('ble-devices-loading', () => {
            console.log('🥁 DeviceInfo :: BLE devices loading');
            this.isBleDevicesLoading = true;
            this.requestUpdate();
        });

        document.addEventListener('ble-devices-error', () => {
            console.log('🥁 DeviceInfo :: BLE devices loading error');
            this.isBleDevicesLoading = false;
            this.requestUpdate();
        });

        document.addEventListener('ble-devices-loaded', (event) => {
            console.log('🥁 DeviceInfo :: BLE devices loaded');
            this.isBleDevicesLoading = false;
            this.requestUpdate();
        });

        // Escuchar el evento de actualización del estado del dispositivo
        document.addEventListener('device-status-update', (event) => {
            console.log('🥁 DeviceInfo :: Device status update', event.detail);
            this.updateDeviceStatus(event.detail.status);
        });

    }

    // Asegurarse de limpiar el intervalo al desconectar el componente
    disconnectedCallback() {
        super.disconnectedCallback();
        if (this.localUptimeInterval) {
            clearInterval(this.localUptimeInterval);
        }
    }

    // Esto solo pasa cuando conectamos al dispositivo
    updateDeviceInfo(info) {
        this.deviceName = info.deviceName;
        this.firmwareInfo = {
            ...this.firmwareInfo,
            ...info.firmwareInfo
        };
        this.deviceStatus = {
            ...this.deviceStatus,
            ...info.deviceStatus
        };

        // Actualizar el Operation Mode
        this.operationMode = info.settings.operationMode;

        // Guardar referencia local al tiempo de uptime
        this.localUptime = this.deviceStatus.uptime;
        this.localPowerOnTime = Date.now() / 1000 - this.deviceStatus.uptime;

        // Actualizar el tiempo de uptime cada segundo
        this.localUptimeInterval = setInterval(() => {
            this.localUptime += 1;
            this.requestUpdate();
        }, 1000);

        this.requestUpdate();
    }

    // Esto pasa cuando el dispositivo envía un nuevo estado
    updateDeviceStatus(status) {
        console.log('Updating device status:', status);
        this.deviceStatus = {
            ...this.deviceStatus,
            ...status
        };
        console.log('Device status updated:', this.deviceStatus);
        this.requestUpdate();
    }


    handleModeChange() {
        this.dispatchEvent(new CustomEvent('mode-change'));
    }

    // Método para formatear el tiempo de uptime
    formatUptime(seconds) {
        const days = Math.floor(seconds / (24 * 3600));
        const hours = Math.floor((seconds % (24 * 3600)) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;

        if (days > 0) {
            return `(${days} Days and ${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(secs).padStart(2, '0')})`;
        } else {
            return `(${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(secs).padStart(2, '0')})`;
        }
    }

    // Método para formatear la fecha de construcción en la zona horaria local
    formatTime(timestamp) {
        const date = new Date(timestamp * 1000);
        const options = {
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hour12: false,
            timeZone: Intl.DateTimeFormat().resolvedOptions().timeZone
        };

        const formattedDate = date.toLocaleString('es-ES', options);
        const [datePart, timePart] = formattedDate.split(', ');

        // Formatear la fecha en YYYY-MM-dd
        const [day, month, year] = datePart.split('/');
        return `${year}-${month}-${day} ${timePart}`;
    }

    render() {
        return html`
            <div id="device-content">

                <ion-list>
                    <!-- General Configuration -->
                    <ion-item-group>
                        <ion-item-divider color="medium">
                            <ion-label>${
                                this.operationMode === 0 ? 'Operation Mode: Off'
                                : this.operationMode === 1 ? 'Capture Status'
                                    : this.operationMode === 2 ? 'Detection Status'
                                        : 'Unknown Operation Mode'
                            }</ion-label>
                        </ion-item-divider>
        
                        <ion-item button id='mode-button-sheet'>
                            <ion-icon slot='start' name="settings-outline" size='small'></ion-icon>
                            <ion-label position="">
                                <h2>Operation Mode</h2>
                                <p>${this.operationMode === 0? 'Off'
                                    : this.operationMode === 1 ? 'Capture (RECON)'
                                        : this.operationMode === 2 ? 'Detection (DEFENSE)'
                                            : 'Unknown Operation Mode'
                                }</p>
                            </ion-label>
                            ${this.isLoading ? html`<ion-spinner slot="end" name="lines-sharp-small"></ion-spinner>` : ''}
                        </ion-item>

                        <ion-item button onClick="${() => this.navigateToSection('networks')}">
                            <ion-icon slot='start' name="wifi-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>WiFi Networks</h2>
                                <p>Networks in range (RSSI >= ${this.settings.minimalRSSI} dBm)</p>
                            </ion-label>
                            ${this.isWifiNetworksLoading ? html`<ion-spinner slot="end" name="lines-sharp-small"></ion-spinner>` : ''}
                            <ion-badge slot='end' class="badge-animate">${this.deviceStatus.wifiNetworks}</ion-badge>
                        </ion-item>

                        <ion-item button onClick="${() => this.navigateToSection('stations')}">
                            <ion-icon slot='start' name="phone-portrait-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>WiFi Devices</h2>
                                <p>Devices in range (RSSI >= ${this.settings.minimalRSSI} dBm)</p>
                            </ion-label>
                            ${this.isWifiDevicesLoading ? html`<ion-spinner slot="end" name="lines-sharp-small"></ion-spinner>` : ''}
                            <ion-badge slot='end' class="badge-animate">${this.deviceStatus.wifiDevices}</ion-badge>
                        </ion-item>

                        <ion-item button onClick="${() => this.navigateToSection('bluetooth')}" class="no-line">
                            <ion-icon slot='start' name="bluetooth-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>BLE Devices</h2>
                                <p>Public devices in range (RSSI >= ${this.settings.minimalRSSI} dBm)</p>
                            </ion-label>
                            ${this.isBleDevicesLoading ? html`<ion-spinner slot="end" name="lines-sharp-small"></ion-spinner>` : ''}
                            <ion-badge slot='end' class="badge-animate">${this.deviceStatus.bleDevices}</ion-badge>
                        </ion-item>

                    </ion-item-group>

                    <ion-item-group>
                        <ion-item-divider color="medium">
                            <ion-label>Device Status</ion-label>
                        </ion-item-divider>

                        <ion-item>
                            <ion-icon slot="start" name="bluetooth-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Device Name</h2>
                                <p>${this.deviceName}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="build-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Version</h2>
                                <p>${this.firmwareInfo.version}</p>
                                <p>MD5: ${this.firmwareInfo.md5}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="scale-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Free Heap Memory</h2>
                                <p>${this.deviceStatus.freeHeap}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="time-outline" size='small'></ion-icon>
                            <ion-label class="no-line">
                                <h2>Uptime</h2>
                                <p>Since ${this.formatTime(this.localPowerOnTime)} - ${this.formatUptime(this.localUptime) || 'N/A'}</p>
                            </ion-label>
                        </ion-item>

                    </ion-item-group>

                    <ion-item-group>
                        <ion-item-divider color="medium">
                            <ion-label>Hardware Information</ion-label>
                        </ion-item-divider>

                        <ion-item>
                            <ion-icon slot="start" name="hardware-chip-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Chip Model</h2>
                                <p>${this.firmwareInfo.chip}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="hardware-chip" size='small'></ion-icon>
                            <ion-label>
                                <h2>Board</h2>
                                <p>${this.firmwareInfo.board}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="speedometer-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>CPU Frequency</h2>
                                <p>${this.firmwareInfo.cpu_mhz} MHz</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="save-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Flash Memory</h2>
                                <p>${(this.firmwareInfo.flash_kb / 1024).toFixed(1)} MB</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="speedometer-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Flash Speed</h2>
                                <p>${this.firmwareInfo.flash_mhz} MHz</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="scale-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>RAM</h2>
                                <p>${this.firmwareInfo.ram_kb} KB</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="wifi-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>WiFi AP MAC Address</h2>
                                <p>${this.firmwareInfo.ap_mac}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="bluetooth-outline" size='small'></ion-icon>
                            <ion-label class="no-line">
                                <h2>BLE MAC Address</h2>
                                <p>${this.firmwareInfo.bt_mac}</p>
                            </ion-label>
                        </ion-item>
                    </ion-item-group>

                    <ion-item-group>
                        <ion-item-divider color="medium">
                            <ion-label>Software Information</ion-label>
                        </ion-item-divider>
                        <ion-item>
                            <ion-icon slot="start" name="construct-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Architecture</h2>
                                <p>${this.firmwareInfo.arch}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="code-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Platform</h2>
                                <p>PlatformIO ${this.firmwareInfo.pio_ver}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="construct-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Framework</h2>
                                <p>Arduino ${this.firmwareInfo.ard_ver}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="document-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Compiler</h2>
                                <p>GCC ${this.firmwareInfo.gcc_ver}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="calendar-outline" size='small'></ion-icon>
                            <ion-label>
                                <h2>Build Time</h2>
                                <p>${this.formatTime(parseInt(this.firmwareInfo.built))}</p>
                            </ion-label>
                        </ion-item>

                        <ion-item>
                            <ion-icon slot="start" name="scale-outline" size='small'></ion-icon>
                            <ion-label class="no-line">
                                <h2>Build Size</h2>
                                <p>${this.firmwareInfo.size_kb} KB</p>
                            </ion-label>
                        </ion-item>
                    </ion-item-group>
                </ion-list>
            </div>
        `;
    }

    static styles = css`
        :host {
            display: block;
            padding: 2px 0;
        }

        ion-item {
            --padding-start: 1rem;
            line-height: 0.8rem;
        }

        ion-icon {
            font-size: 16px; /* Tamaño pequeño para los íconos */
        }

        ion-item-group {
            margin-bottom: 16px !important;
        }

        ion-label[position="floating"] {
            margin-bottom: 1em !important;
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

        ion-label>h2 {
            margin-left: 0;
            margin-right: 0;
            margin-top: 1rem !important;
            margin-bottom: 0.5rem !important;
            font-size: 1rem;
            font-weight: bold;
        }

        ion-content {
            --overflow: scroll;
            --padding-bottom: 20px;
        }

        ion-content::part(scroll) {
            overflow-y: auto !important;
        }

        ion-item-divider {
            background: var(--ion-color-medium);
            color: var(--ion-color-light);
        }

        ion-item-group {
            margin-bottom: 16px !important;
        }

        .loading-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100%;
            padding: 2rem;
        }

        .loading-container p {
            margin-top: 1rem;
            color: var(--ion-color-medium);
        }
    `;

}

customElements.define('sneak-device-info', SneakDeviceInfo);

