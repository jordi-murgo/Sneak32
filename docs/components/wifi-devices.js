import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakWifiDevices extends LitElement {
    static properties = {
        devices: { type: Array },
        isLoadning: { type: Boolean },
        deviceStatus: { type: Object }
    };

    constructor() {
        super();
        this.devices = [];
        this.isLoadning = false;
        this.deviceStatus = { devices: 0 };
        this.remoteTimestamp = 0;
        this.localTimestamp = 0;
        this.networks = {};

        document.addEventListener('wifi-devices-loading', () => {
            console.log('🎯 WifiDevices :: Devices loading');
            this.isLoadning = true;
        });

        document.addEventListener('wifi-devices-loaded', (event) => {
            console.log('🎯 WifiDevices :: Devices loaded', event.detail);
            this.isLoadning = false;
            this.devices = event.detail.stations;
            this.remoteTimestamp = event.detail.timestamp * 1000;
            this.localTimestamp = Date.now();

            this.devices.forEach(station => {
                station.vendor = window.vendorDecode(station.mac);
                station.is_public = !this.isLocalMac(station.mac);
            });

            this.requestUpdate();
        });

        document.addEventListener('wifi-networks-loading', () => {
            console.log('🎯 WifiDevices :: Networks loading');
            this.isLoadning = true;
        });

        document.addEventListener('wifi-networks-loaded', (event) => {
            console.log('🥁 WifiNetworks :: WiFi networks loaded', event.detail);
            this.isLoadning = false;
            event.detail.ssids.forEach(network => {
                if(network.mac !== "FF:FF:FF:FF:FF:FF" && network.mac !== "00:00:00:00:00:00") {
                    this.networks[network.mac] = network.ssid;
                }
            });
            this.requestUpdate();
        });

        document.addEventListener('device-status-update', (event) => {
            console.log('🎯 WifiDevices :: Device status update', event.detail);
            this.updateDeviceStatus(event.detail.status);
        });
    }

    updateDeviceStatus(status) {
        this.deviceStatus = {
            ...this.deviceStatus,
            ...status
        };
        this.requestUpdate();
    }

    getLastSeenDate(lastSeen) {
        const difference = this.remoteTimestamp - lastSeen * 1000;
        const date = new Date(this.localTimestamp - difference);

        // Return the date in the format of "2024-12-31 23:30:00"
        // Obtenir les parts de la data
        var year = date.getFullYear();

        // Afegir 1 al mes ja que getMonth() retorna valors de 0 a 11
        var month = ('0' + (date.getMonth() + 1)).slice(-2);

        var day = ('0' + date.getDate()).slice(-2);
        var hours = ('0' + date.getHours()).slice(-2);
        var minutes = ('0' + date.getMinutes()).slice(-2);
        var seconds = ('0' + date.getSeconds()).slice(-2);

        // Construir la cadena de data en el format desitjat
        return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
    }

    isLocalMac(mac) {
        const firstByte = parseInt(mac.split(':')[0], 16);
        return (firstByte & 0x02) === 0x02;
    }

    render() {
        return html`
            <div class="wifi-devices-content">
                ${this.isLoadning ? html`
                    <div class="loading-overlay">
                        <ion-spinner></ion-spinner>
                        <ion-label>Loadning...</ion-label>
                    </div>
                ` : ''}
                
                <ion-list>
                    <ion-item>
                        <ion-label>
                            <h2>WiFi Devices</h2>
                            <p>Downloaded: ${this.devices.length}, Scanned: ${this.deviceStatus.wifiDevices}</p>
                        </ion-label>
                    </ion-item>
                    <!-- Actions -->
                    <ion-item>
                        <ion-button 
                            @click=${this.startLoad}
                            ?disabled=${this.isLoadning}
                        >
                            <ion-icon slot="start" name="refresh-outline"></ion-icon>
                            Reload
                        </ion-button>
                        <ion-button   
                            @click=${this.downloadDevices}
                            ?disabled=${this.isLoadning}
                        >
                            <ion-icon slot="start" name="download-outline"></ion-icon>
                            Download
                        </ion-button>
                        <ion-button 
                            color="warning"
                            @click=${this.saveDevices}
                            ?disabled=${this.isLoadning}
                        >
                            <ion-icon slot="start" name="save-outline"></ion-icon>
                            Save
                        </ion-button>
                        <ion-button 
                            color="danger"
                            @click=${this.deleteDevices}
                            ?disabled=${this.isLoadning}
                        >
                            <ion-icon slot="start" name="trash-outline"></ion-icon>
                            Delete
                        </ion-button>
                    </ion-item>

                    ${this.renderDevicesList()}
                </ion-list>
            </div>
        `;
    }

    renderDevicesList() {
        const sortedDevices = [...this.devices].sort((a, b) => b.rssi - a.rssi);

        return sortedDevices.map(station => {
            const isAp = (station.mac === station.bssid);
            var ssid = this.networks[station.bssid];
            
            return html`
                <ion-item-sliding>
                    <ion-item>
                        <ion-icon 
                            slot="start" 
                            name="${isAp ? 'radio-outline' : 'phone-portrait-outline'}"
                            style="color: ${this.getSignalColor(station.rssi)}"
                        ></ion-icon>
                        <ion-label>
                            <h3>MAC: ${station.mac}${isAp ? html` (AP)` : ''}${ssid ? `, Network: ${ssid}` : ''}</h3>
                            <p>Channel: ${station.channel}, RSSI: ${station.rssi} dBm ${station.vendor ? html`(Vendor: ${station.vendor})` : ''}</p>
                            <p>Last seen: ${this.getLastSeenDate(station.last_seen)}</p>
                        </ion-label>
                        <div class="badge-container">
                            ${station.rssi <= -90 ? html`
                                <ion-badge color="warning">Weak</ion-badge>
                            ` : ''}
                            ${!station.is_public ? html`
                                <ion-badge color="tertiary">Local Addr</ion-badge>
                            ` : ''}
                            <ion-badge color="primary">${station.times_seen}</ion-badge>
                        </div>
                    </ion-item>
                    <ion-item-options side="end">
                        <ion-item-option color="danger" @click=${() => this.deleteStation(station.mac)}>
                            <ion-icon slot="start" name="trash-outline"></ion-icon>
                            Delete
                        </ion-item-option>
                    </ion-item-options>
                </ion-item-sliding>
            `;
        });
    }

    downloadDevices() {
        // Prepare data to export
        const exportData = this.devices.map(device => ({
            ...device,
            network: this.networks[device.bssid] || null,
            last_seen_formatted: this.getLastSeenDate(device.last_seen)
        }));

        // Create JSON content
        const jsonContent = JSON.stringify(exportData, null, 2);
        
        // Generate filename with current date/time
        const now = new Date();
        const timestamp = `${now.getFullYear()}${String(now.getMonth() + 1).padStart(2, '0')}${String(now.getDate()).padStart(2, '0')}-${String(now.getHours()).padStart(2, '0')}${String(now.getMinutes()).padStart(2, '0')}${String(now.getSeconds()).padStart(2, '0')}`;
        const filename = `wifi-devices-${timestamp}.json`;

        // Create blob and download link
        const blob = new Blob([jsonContent], { type: 'application/json' });
        const url = window.URL.createObjectURL(blob);
        const link = document.createElement('a');
        link.href = url;
        link.download = filename;
        
        // Trigger download
        document.body.appendChild(link);
        link.click();
        
        // Cleanup
        document.body.removeChild(link);
        window.URL.revokeObjectURL(url);
    }

    saveDevices() {
        this.dispatchEvent(new CustomEvent('save-wifi-devices', {
            bubbles: true,
            composed: true
        }));
    }

    deleteDevices() {
        this.dispatchEvent(new CustomEvent('delete-all-devices', {
            bubbles: true,
            composed: true
        }));
    }

    deleteStation(mac) {
        this.devices = this.devices.filter(station => station.mac !== mac);
        this.dispatchEvent(new CustomEvent('delete-station', {
            detail: { mac },
            bubbles: true,
            composed: true
        }));
    }

    getSignalColor(dBm) {
        // Assegurem que el valor estigui entre -50 i -100
        if (dBm > -50) dBm = -50;
        if (dBm < -100) dBm = -100;
    
        // Convertim el valor de dBm a un rang de 0 a 1
        let ratio = (-dBm - 50) / 50; // -50 serà 0, -100 serà 1
        let r, g, b;
    
        if (ratio <= 0.3333) {
            // Segment 1: de verd/cian a groc
            let segmentRatio = ratio / 0.3333;
            r = Math.round(255 * segmentRatio);
            g = 255;
            b = Math.round(255 * (1 - segmentRatio));
        } else if (ratio <= 0.6666) {
            // Segment 2: de groc a vermell
            let segmentRatio = (ratio - 0.3333) / 0.3333;
            r = 255;
            g = Math.round(255 * (1 - segmentRatio));
            b = 0;
        } else {
            // Segment 3: de vermell a gris
            let segmentRatio = (ratio - 0.6666) / 0.3334;
            r = Math.round(255 + (21 - 255) * segmentRatio);
            g = Math.round(0 + (210 - 0) * segmentRatio);
            b = Math.round(0 + (210 - 0) * segmentRatio);
        }
    
        // Asegurar que los valores RGB estén dentro del rango válido (0-255)
        r = Math.max(0, Math.min(255, r));
        g = Math.max(0, Math.min(255, g));
        b = Math.max(0, Math.min(255, b));
    
        return `rgb(${r},${g},${b})`;
    }

    startLoad() {
        this.dispatchEvent(new CustomEvent('load-wifi-devices', {
            bubbles: true,
            composed: true
        }));
    }

    static styles = css`
        :host {
            display: block;
            padding: 2px 0;
        }

        .wifi-devices-content {
            margin: 0 auto;
        }

        ion-spinner {
            margin-right: 1rem;
        }

        .badge-container {
            display: flex;
            align-items: center;
            gap: 5px;
            margin-left: 8px;
        }

        ion-badge {
            text-transform: capitalize;
            padding: 4px 8px;
        }

        ion-badge[color="success"] {
            background: var(--ion-color-success, #2dd36f);
            color: var(--ion-color-success-contrast, #ffffff);
        }

        ion-badge[color="primary"] {
            background: var(--ion-color-primary, #3880ff);
            color: var(--ion-color-primary-contrast, #ffffff);
        }

        ion-badge[color="warning"] {
            background: var(--ion-color-warning, #ffc409);
            color: var(--ion-color-warning-contrast, #000000);
        }

        ion-badge[color="tertiary"] {
            background: var(--ion-color-tertiary, #6030ff);
            color: var(--ion-color-tertiary-contrast, #ffffff);
        }

        ion-item-option[color="danger"] {
            --background: var(--ion-color-danger);
            --color: var(--ion-color-danger-contrast);
            --ion-color-base: var(--ion-color-danger);
            --ion-color-contrast: var(--ion-color-danger-contrast);
        }

        ion-item-option ion-icon {
            margin-right: 8px;
            font-size: 1.2em;
        }

        ion-badge {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            min-height: 20px;
            min-width: 20px;
            border-radius: 4px;
        }

        ion-item-option {
            --button-color: var(--color);
        }

        ion-button {
            --padding-start: 12px;
            --padding-end: 12px;
            --padding-top: 8px;
            --padding-bottom: 8px;
        }

        ion-button[color="warning"] {
            --ion-color-base: var(--ion-color-warning, #ffc409);
            --ion-color-contrast: var(--ion-color-warning-contrast, #000000);
        }

        ion-button[color="danger"] {
            --ion-color-base: var(--ion-color-danger, #eb445a);
            --ion-color-contrast: var(--ion-color-danger-contrast, #ffffff);
        }

        .loading-overlay {
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: rgba(0, 0, 0, 0.8);
            padding: 20px;
            border-radius: 8px;
            z-index: 9999;
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 10px;
        }

        .loading-overlay ion-spinner {
            --color: white;
            margin: 0;
        }

        .loading-overlay ion-label {
            color: white;
            margin: 0;
        }
    `;
}

customElements.define('sneak-wifi-devices', SneakWifiDevices); 