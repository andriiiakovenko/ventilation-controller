#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Ventilation Controller</title>
    <style>
        :root {
            --bg-body: #161b22;
            --bg-sidebar: #0f141a;
            --sidebar-item: #182029;
            --sidebar-item-hover: #222c38;
            --sidebar-border: #25303e;
            --main-bg: #1a212b;
            --card-header: linear-gradient(180deg, #3d4756 0%, #29323e 100%);
            --card-bg: #202732;
            --table-th: #27313e;
            --table-td-alt: #171d25;
            --border-color: #354252;
            --accent-blue: #38bdf8;
            --accent-blue-hover: #7dd3fc;
            --accent-green: #22c55e;
            --accent-orange: #f59e0b;
            --accent-red: #ef4444;
            --text-main: #f1f5f9;
            --text-muted: #94a3b8;
            --text-dim: #64748b;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            -webkit-tap-highlight-color: transparent;
        }

        body {
            background-color: var(--bg-body);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
        }

        /* Top Header */
        .top-navbar {
            background: linear-gradient(180deg, #1d2530 0%, #12171e 100%);
            border-bottom: 2px solid #2d3846;
            padding: 10px 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            height: 60px;
        }

        .top-left {
            display: flex;
            align-items: center;
            gap: 15px;
        }

        .menu-toggle {
            display: none;
            background: none;
            border: none;
            color: var(--text-main);
            font-size: 1.5rem;
            cursor: pointer;
        }

        .brand-logo {
            display: flex;
            align-items: center;
            gap: 10px;
            font-size: 1.15rem;
            font-weight: 700;
            color: #fff;
            letter-spacing: -0.01em;
        }

        .brand-icon {
            font-size: 1.4rem;
        }

        .brand-model {
            font-size: 0.72rem;
            background: #273342;
            color: var(--accent-blue);
            padding: 3px 8px;
            border-radius: 4px;
            border: 1px solid #374659;
            font-weight: 600;
        }

        .top-right {
            display: flex;
            align-items: center;
            gap: 16px;
            font-size: 0.82rem;
            color: var(--text-muted);
        }

        .top-status-badge {
            display: flex;
            align-items: center;
            gap: 6px;
            padding: 4px 10px;
            background: rgba(34, 197, 94, 0.15);
            border: 1px solid rgba(34, 197, 94, 0.4);
            border-radius: 12px;
            color: #4ade80;
            font-weight: 600;
        }

        .top-status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: #22c55e;
            box-shadow: 0 0 8px #22c55e;
        }

        .btn-top-action {
            background: linear-gradient(180deg, #374353 0%, #222b36 100%);
            border: 1px solid #455467;
            color: #d1dbe5;
            padding: 6px 14px;
            border-radius: 6px;
            font-size: 0.8rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease;
        }

        .btn-top-action:hover {
            border-color: var(--accent-blue);
            color: #fff;
        }

        /* 2-Column Layout */
        .layout-container {
            display: flex;
            flex: 1;
            min-height: calc(100vh - 60px);
        }

        /* Left Navigation Sidebar (Extensible for future options) */
        .sidebar {
            width: 240px;
            background: var(--bg-sidebar);
            border-right: 1px solid #222a36;
            padding: 16px 10px;
            display: flex;
            flex-direction: column;
            gap: 20px;
            flex-shrink: 0;
        }

        .nav-category {
            font-size: 0.7rem;
            font-weight: 700;
            color: #64748b;
            text-transform: uppercase;
            letter-spacing: 0.08em;
            padding: 0 10px 5px 10px;
            border-bottom: 1px solid #1e2632;
            margin-bottom: 6px;
        }

        .nav-list {
            list-style: none;
            display: flex;
            flex-direction: column;
            gap: 4px;
        }

        .nav-item {
            display: flex;
            align-items: center;
            gap: 12px;
            padding: 10px 12px;
            border-radius: 6px;
            font-size: 0.85rem;
            font-weight: 600;
            color: #94a3b8;
            cursor: pointer;
            transition: all 0.15s ease;
            border: 1px solid transparent;
            user-select: none;
        }

        .nav-item:hover {
            background: var(--sidebar-item-hover);
            color: #fff;
            border-color: #354252;
        }

        .nav-item.active {
            background: linear-gradient(90deg, #222d3b 0%, #19212c 100%);
            color: #fff;
            border-color: var(--accent-blue);
            box-shadow: inset 3px 0 0 var(--accent-blue);
        }

        .nav-icon {
            width: 20px;
            height: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1.1rem;
        }

        /* Main Viewport */
        .main-viewport {
            flex: 1;
            padding: 20px 24px;
            background: var(--main-bg);
            overflow-y: auto;
        }

        /* Tab Header Bar */
        .tabs-header-bar {
            display: flex;
            align-items: center;
            gap: 4px;
            border-bottom: 2px solid #334152;
            margin-bottom: 20px;
            overflow-x: auto;
        }

        .tab-btn {
            background: #202732;
            color: #94a3b8;
            border: 1px solid #334050;
            border-bottom: none;
            padding: 10px 20px;
            font-size: 0.85rem;
            font-weight: 600;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            cursor: pointer;
            transition: all 0.2s ease;
            white-space: nowrap;
        }

        .tab-btn:hover {
            background: #2a3443;
            color: #fff;
        }

        .tab-btn.active {
            background: #2e3a4a;
            color: #fff;
            border-color: #425266;
            border-top: 3px solid var(--accent-blue);
            font-weight: 700;
        }

        .page-title-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 16px;
        }

        .page-title-row h2 {
            font-size: 1.2rem;
            font-weight: 700;
            color: #fff;
        }

        /* Note Box */
        .note-card {
            background: #171d25;
            border: 1px solid #2c3644;
            border-radius: 6px;
            padding: 14px 18px;
            margin-bottom: 20px;
            font-size: 0.83rem;
            line-height: 1.55;
            color: #cbd5e1;
        }

        .note-card b.note-label {
            color: #fff;
            display: block;
            margin-bottom: 8px;
            font-size: 0.88rem;
        }

        .note-card ol {
            padding-left: 20px;
        }

        .note-card ol li {
            margin-bottom: 5px;
        }

        /* Card Panels & Tables */
        .panel-card {
            background: var(--card-bg);
            border: 1px solid var(--border-color);
            border-radius: 6px;
            margin-bottom: 20px;
            overflow: hidden;
            box-shadow: 0 4px 16px rgba(0,0,0,0.25);
        }

        .panel-header {
            background: var(--card-header);
            padding: 11px 16px;
            font-size: 0.9rem;
            font-weight: 700;
            color: #fff;
            border-bottom: 1px solid var(--border-color);
            letter-spacing: 0.02em;
        }

        .panel-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 0.83rem;
        }

        .panel-table tr {
            border-bottom: 1px solid #283340;
        }

        .panel-table tr:last-child {
            border-bottom: none;
        }

        .panel-table td.col-label {
            width: 250px;
            background: #1a212a;
            padding: 12px 16px;
            font-weight: 600;
            color: #cbd5e1;
            border-right: 1px solid #283340;
            vertical-align: middle;
        }

        .panel-table td.col-val {
            background: #161c24;
            padding: 12px 16px;
            color: #f1f5f9;
            vertical-align: middle;
        }

        .panel-table tr.subhead-row td {
            background: linear-gradient(180deg, #2d3846 0%, #202832 100%);
            padding: 10px 16px;
            font-weight: 700;
            color: var(--accent-blue);
            border-top: 1px solid #384656;
            border-bottom: 1px solid #384656;
        }

        /* Buttons & Links */
        .btn-modern {
            background: linear-gradient(180deg, #384554 0%, #242c36 100%);
            border: 1px solid #4a596c;
            color: #e2e8f0;
            padding: 6px 16px;
            border-radius: 4px;
            font-size: 0.8rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.15s ease;
        }

        .btn-modern:hover {
            background: linear-gradient(180deg, #47576b 0%, #2c3643 100%);
            border-color: var(--accent-blue);
            color: #fff;
        }

        .btn-update {
            display: inline-flex;
            align-items: center;
            gap: 6px;
            background: rgba(56, 189, 248, 0.15);
            border: 1px solid rgba(56, 189, 248, 0.4);
            color: var(--accent-blue);
            padding: 5px 14px;
            border-radius: 4px;
            font-size: 0.8rem;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.15s ease;
        }

        .btn-update:hover {
            background: rgba(56, 189, 248, 0.25);
            border-color: var(--accent-blue);
            color: #fff;
        }

        .fw-ver-tag {
            color: var(--accent-blue);
            font-weight: 700;
            text-decoration: underline;
            cursor: pointer;
        }

        /* Modal Upgrade Window */
        .modal-overlay {
            position: fixed;
            inset: 0;
            background: rgba(0, 0, 0, 0.75);
            display: none;
            justify-content: center;
            align-items: center;
            z-index: 1000;
            backdrop-filter: blur(4px);
        }

        .modal-window {
            background: #1e2632;
            border: 2px solid #384658;
            border-radius: 8px;
            width: 90%;
            max-width: 580px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.7);
            overflow: hidden;
            display: flex;
            flex-direction: column;
        }

        .modal-header {
            background: linear-gradient(180deg, #3d4959 0%, #27303c 100%);
            padding: 12px 18px;
            font-size: 0.95rem;
            font-weight: 700;
            color: #fff;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #435264;
        }

        .modal-close {
            background: none;
            border: none;
            color: #94a3b8;
            font-size: 1.25rem;
            cursor: pointer;
        }

        .modal-close:hover {
            color: #fff;
        }

        .modal-body {
            padding: 20px;
            display: flex;
            flex-direction: column;
            gap: 16px;
        }

        .modal-info-box {
            background: #141a22;
            border: 1px solid #2a3442;
            border-radius: 6px;
            padding: 12px 14px;
            font-size: 0.82rem;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 8px;
        }

        .modal-info-item span.label {
            color: var(--text-muted);
            display: block;
            font-size: 0.72rem;
            text-transform: uppercase;
        }

        .modal-info-item span.val {
            color: var(--accent-blue);
            font-weight: 700;
        }

        .drop-zone {
            border: 2px dashed #404f62;
            border-radius: 8px;
            padding: 24px;
            text-align: center;
            background: #161c24;
            cursor: pointer;
            transition: all 0.2s ease;
        }

        .drop-zone:hover {
            border-color: var(--accent-blue);
            background: #1a222c;
        }

        .modal-progress {
            display: none;
            flex-direction: column;
            gap: 8px;
        }

        .progress-bar-track {
            background: #11151c;
            border: 1px solid #2e3b4a;
            height: 14px;
            border-radius: 7px;
            overflow: hidden;
        }

        .progress-bar-fill {
            background: linear-gradient(90deg, #38bdf8 0%, #22c55e 100%);
            width: 0%;
            height: 100%;
            transition: width 0.15s ease;
        }

        .modal-footer {
            background: #151a22;
            padding: 12px 20px;
            border-top: 1px solid #2a3442;
            display: flex;
            justify-content: flex-end;
            gap: 10px;
        }

        /* Dashboard Styles */
        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 16px;
            margin-bottom: 20px;
        }

        .btn-speed-group {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 8px;
            margin-top: 10px;
        }

        .btn-speed {
            background: #182029;
            border: 1px solid #334050;
            color: #94a3b8;
            padding: 12px 6px;
            border-radius: 6px;
            font-size: 0.85rem;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.2s ease;
            text-align: center;
        }

        .btn-speed:hover {
            border-color: var(--accent-blue);
            color: #fff;
        }

        .btn-speed.active-0 {
            background: rgba(239, 68, 68, 0.2);
            border-color: #ef4444;
            color: #f87171;
            box-shadow: 0 0 10px rgba(239, 68, 68, 0.4);
        }

        .btn-speed.active-1 {
            background: rgba(56, 189, 248, 0.2);
            border-color: #38bdf8;
            color: #38bdf8;
            box-shadow: 0 0 10px rgba(56, 189, 248, 0.4);
        }

        .btn-speed.active-2 {
            background: rgba(34, 197, 94, 0.2);
            border-color: #22c55e;
            color: #22c55e;
            box-shadow: 0 0 10px rgba(34, 197, 94, 0.4);
        }

        .btn-speed.active-3 {
            background: rgba(245, 158, 11, 0.2);
            border-color: #f59e0b;
            color: #f59e0b;
            box-shadow: 0 0 10px rgba(245, 158, 11, 0.4);
        }

        .valve-item-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 14px;
            background: #151b22;
            border: 1px solid #2a3442;
            border-radius: 6px;
            margin-bottom: 8px;
        }

        .valve-item-row.open {
            border-color: var(--accent-blue);
            background: rgba(56, 189, 248, 0.08);
        }

        /* Toggle switch */
        .switch-toggle {
            position: relative;
            display: inline-block;
            width: 50px;
            height: 26px;
        }

        .switch-toggle input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .slider-round {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #2a3442;
            transition: .25s ease;
            border-radius: 26px;
            border: 1px solid #415165;
        }

        .slider-round::before {
            position: absolute;
            content: "";
            height: 18px;
            width: 18px;
            left: 3px;
            bottom: 3px;
            background-color: #fff;
            transition: .25s ease;
            border-radius: 50%;
        }

        .switch-toggle input:checked + .slider-round {
            background-color: var(--accent-blue);
            border-color: var(--accent-blue);
        }

        .switch-toggle input:checked + .slider-round::before {
            transform: translateX(24px);
        }

        .sensor-card {
            background: #161c24;
            border: 1px solid #283340;
            border-radius: 6px;
            padding: 14px;
            display: flex;
            flex-direction: column;
            gap: 10px;
        }

        .sensor-card-top {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .badge-status {
            font-size: 0.7rem;
            padding: 2px 7px;
            border-radius: 10px;
            font-weight: 700;
        }

        .badge-online {
            background: rgba(34, 197, 94, 0.2);
            color: #4ade80;
            border: 1px solid rgba(34, 197, 94, 0.4);
        }

        .badge-offline {
            background: rgba(239, 68, 68, 0.2);
            color: #f87171;
            border: 1px solid rgba(239, 68, 68, 0.4);
        }

        .sensor-metrics-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 8px;
            background: #11161d;
            padding: 10px;
            border-radius: 6px;
        }

        .sensor-metric-val {
            font-size: 1.15rem;
            font-weight: 700;
            color: var(--accent-blue);
        }

        .sensor-metric-label {
            font-size: 0.7rem;
            color: var(--text-muted);
        }

        @media (max-width: 860px) {
            .menu-toggle {
                display: block;
            }
            .sidebar {
                position: fixed;
                left: -260px;
                top: 60px;
                bottom: 0;
                z-index: 99;
                transition: left 0.25s ease;
                box-shadow: 4px 0 20px rgba(0,0,0,0.5);
            }
            .sidebar.open {
                left: 0;
            }
            .panel-table td.col-label {
                width: 130px;
                padding: 10px;
            }
            .panel-table td.col-val {
                padding: 10px;
            }
            .main-viewport {
                padding: 14px;
            }
        }
    </style>
</head>
<body>
    <!-- Top Bar -->
    <header class="top-navbar">
        <div class="top-left">
            <button class="menu-toggle" onclick="toggleSidebar()">&#9776;</button>
            <div class="brand-logo">
                <span class="brand-icon">&#127744;</span>
                <span>Ventilation Controller</span>
                <span class="brand-model">ESP32 Master</span>
            </div>
        </div>
        <div class="top-right">
            <div class="top-status-badge">
                <span class="top-status-dot"></span>
                <span id="top-conn-text">System Active</span>
            </div>
            <button class="btn-top-action" onclick="restartController()">Restart</button>
        </div>
    </header>

    <!-- Layout Container: Extensible Left Navigation Panel + Main Viewport -->
    <div class="layout-container">
        <!-- Left Sidebar Navigation -->
        <nav class="sidebar" id="main-sidebar">
            <!-- Section 1: Control & Monitoring -->
            <div>
                <div class="nav-category">Control & Monitoring</div>
                <ul class="nav-list">
                    <li class="nav-item active" id="nav-dashboard" onclick="switchView('dashboard')">
                        <span class="nav-icon">&#128202;</span>
                        <span>Dashboard</span>
                    </li>
                    <li class="nav-item" id="nav-sensors" onclick="switchView('sensors')">
                        <span class="nav-icon">&#128225;</span>
                        <span>Sensor Nodes</span>
                    </li>
                </ul>
            </div>

            <!-- Section 2: Management & Settings (Extensible for future options) -->
            <div>
                <div class="nav-category">Settings</div>
                <ul class="nav-list">
                    <!-- Administration option -->
                    <li class="nav-item" id="nav-admin" onclick="switchView('admin')">
                        <span class="nav-icon">&#9881;</span>
                        <span>Administration</span>
                    </li>
                    <li class="nav-item" id="nav-syslog" onclick="switchView('syslog')">
                        <span class="nav-icon">&#128196;</span>
                        <span>System Log</span>
                    </li>
                </ul>
            </div>
        </nav>

        <!-- Main Viewport Content Area -->
        <main class="main-viewport">
            <!-- ================= VIEW 1: ADMINISTRATION (FIRMWARE UPGRADE) ================= -->
            <div id="view-admin" style="display:none;">
                <!-- Top Tabs Bar -->
                <div class="tabs-header-bar">
                    <button class="tab-btn active" id="tab-btn-fw" onclick="selectAdminTab('firmware')">Firmware Upgrade</button>
                    <button class="tab-btn" id="tab-btn-sys" onclick="selectAdminTab('system')">System Info</button>
                    <button class="tab-btn" id="tab-btn-reboot" onclick="restartController()">Reboot</button>
                </div>

                <div class="page-title-row">
                    <h2>Administration - Firmware Upgrade</h2>
                </div>

                <!-- Info Note -->
                <div class="note-card">
                    <b class="note-label">Firmware Upgrade Notes:</b>
                    <ol>
                        <li>Flash newly compiled <code>firmware.bin</code> files wirelessly over OTA without USB connection.</li>
                        <li>Configuration parameters and relay safety state will be maintained across upgrades.</li>
                        <li>Ensure you select the binary compiled for the corresponding architecture (ESP32 for Controller, ESP32-C3 for Sensor Nodes).</li>
                        <li>The module automatically verifies checksums and restarts upon successful upload.</li>
                    </ol>
                </div>

                <!-- Firmware Version Card (MODULE LIST & OTA BUTTONS) -->
                <div class="panel-card">
                    <div class="panel-header">Connected Modules & Firmware Status</div>
                    <table class="panel-table">
                        <!-- Main Controller Section -->
                        <tr class="subhead-row">
                            <td colspan="2">Main Controller Module</td>
                        </tr>
                        <tr>
                            <td class="col-label">
                                <div style="font-weight:700; color:#fff;">Ventilation Master</div>
                                <div style="font-size:0.75rem; color:#94a3b8;">ESP32 Dev Module</div>
                            </td>
                            <td class="col-val">
                                <div style="display:flex; flex-direction:column; gap:6px;">
                                    <div>
                                        Current Version : 
                                        <span class="fw-ver-tag" id="ctrl-cur-ver">v0.0.1</span>
                                        <span style="font-size:0.75rem; color:var(--text-muted); margin-left:8px;" id="ctrl-extra-info">
                                            (Partition: ota_0)
                                        </span>
                                    </div>
                                    <div style="font-size:0.8rem; color:var(--text-muted);">
                                        Status: <span style="color:#4ade80; font-weight:700;">● Online</span> &bull; 
                                        IP: <span id="ctrl-ip" style="color:#e2e8f0;">192.168.51.100</span>
                                    </div>
                                    <div style="margin-top:2px;">
                                        <button class="btn-update" onclick="openOtaModal('main')">
                                            &#11014; Update Firmware
                                        </button>
                                    </div>
                                </div>
                            </td>
                        </tr>

                        <!-- Sensor Modules Section -->
                        <tr class="subhead-row">
                            <td colspan="2">Sensor Modules</td>
                        </tr>
                        <tbody id="sensor-modules-tbody">
                            <tr>
                                <td colspan="2" style="padding:14px; text-align:center; color:var(--text-muted);">
                                    Discovering connected sensor nodes...
                                </td>
                            </tr>
                        </tbody>
                    </table>
                </div>

                <!-- Qingping Direct Wi-Fi Binding & Auto-Discovery Card -->
                <div class="panel-card" id="qp-admin-card">
                    <div class="panel-header">Qingping Air Monitor Lite (CGDN1) — Direct Wi-Fi Connection</div>
                    <div style="padding:16px;">
                        <div class="note-card" style="margin-bottom:14px;">
                            <b class="note-label">Zero Single-Point-of-Failure Setup (Direct to Ventilation Controller):</b>
                            <ol>
                                <li>Open <a href="https://developer.qingping.co" target="_blank" style="color:var(--accent-blue);">developer.qingping.co</a> (Private Access Config &rarr; Add Configuration &rarr; <b>Self-built MQTT</b>).</li>
                                <li>Set <b>Broker IP</b> to <code id="qp-broker-ip" style="color:#4ade80; font-weight:700;">192.168.51.100</code>, <b>Port</b>: <code>1883</code>, <b>Up Topic</b>: <code>qingping/{mac}/up</code>, <b>Down Topic</b>: <code>qingping/{mac}/down</code>.</li>
                                <li>As soon as each Qingping connects over Wi-Fi, it will appear under <b>Discovered Qingping Monitors</b> below (and automatically switch to 15-second live reporting). Click a room button to bind it!</li>
                            </ol>
                        </div>

                        <table class="panel-table" style="border:1px solid #283340; margin-bottom:16px;">
                            <tr class="subhead-row">
                                <td colspan="2">Room &larr; Qingping MAC Assignments</td>
                            </tr>
                            <tr>
                                <td class="col-label">
                                    <div style="font-weight:700; color:#fff;">Danylo Room</div>
                                    <div style="font-size:0.75rem; color:var(--accent-blue);">Stue Air Monitor (Qingping)</div>
                                </td>
                                <td class="col-val">
                                    <div style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
                                        <input type="text" id="qp-mac-danylo" placeholder="e.g. CC:B5:D1:32:21:0E"
                                               style="background:#11161d; border:1px solid #354252; color:#fff; padding:6px 10px; border-radius:4px; font-family:monospace; width:190px;">
                                        <button class="btn-top-action" onclick="saveQingpingMac('danylo')">Save MAC</button>
                                        <span id="qp-cur-danylo" style="font-size:0.78rem; color:var(--text-muted);"></span>
                                    </div>
                                </td>
                            </tr>
                            <tr>
                                <td class="col-label">
                                    <div style="font-weight:700; color:#fff;">Anastasiia Room</div>
                                    <div style="font-size:0.75rem; color:var(--accent-blue);">Kontor Air Monitor (Qingping)</div>
                                </td>
                                <td class="col-val">
                                    <div style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
                                        <input type="text" id="qp-mac-anastasiia" placeholder="e.g. CC:B5:D1:32:1C:16"
                                               style="background:#11161d; border:1px solid #354252; color:#fff; padding:6px 10px; border-radius:4px; font-family:monospace; width:190px;">
                                        <button class="btn-top-action" onclick="saveQingpingMac('anastasiia')">Save MAC</button>
                                        <span id="qp-cur-anastasiia" style="font-size:0.78rem; color:var(--text-muted);"></span>
                                    </div>
                                </td>
                            </tr>
                            <tr>
                                <td class="col-label">
                                    <div style="font-weight:700; color:#fff;">Parents Room</div>
                                    <div style="font-size:0.75rem; color:var(--accent-blue);">Foreldre Air Monitor (Qingping)</div>
                                </td>
                                <td class="col-val">
                                    <div style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
                                        <input type="text" id="qp-mac-parents" placeholder="e.g. CC:B5:D1:32:23:41"
                                               style="background:#11161d; border:1px solid #354252; color:#fff; padding:6px 10px; border-radius:4px; font-family:monospace; width:190px;">
                                        <button class="btn-top-action" onclick="saveQingpingMac('parents')">Save MAC</button>
                                        <span id="qp-cur-parents" style="font-size:0.78rem; color:var(--text-muted);"></span>
                                    </div>
                                </td>
                            </tr>
                        </table>

                        <div style="font-size:0.84rem; font-weight:700; color:#fff; margin-bottom:8px;">
                            Auto-Discovered Qingping Monitors (<span id="qp-client-count">0</span> direct TCP clients):
                        </div>
                        <div id="qp-discovered-list" style="font-size:0.8rem; color:var(--text-muted); margin-bottom:14px;">
                            Waiting for Qingping monitors to report on port 1883...
                        </div>

                        <div style="font-size:0.84rem; font-weight:700; color:#fff; margin-bottom:6px;">
                            Port 1883 Live Broker Activity Log:
                        </div>
                        <pre id="qp-broker-logs" style="background:#0f141a; border:1px solid #283340; border-radius:6px; padding:10px 12px; font-family:monospace; font-size:0.75rem; color:#94a3b8; white-space:pre-wrap; word-break:break-all; max-height:180px; overflow-y:auto;">No activity yet...</pre>
                    </div>
                </div>
            </div>

            <!-- ================= VIEW 2: DASHBOARD (VENTILATION CONTROLS) ================= -->
            <div id="view-dashboard">
                <div class="page-title-row">
                    <h2>Dashboard - Ventilation Controls</h2>
                    <div>
                        <button class="btn-modern" id="dash-mode-btn" onclick="toggleMode()">Mode: AUTO</button>
                    </div>
                </div>

                <div class="dashboard-grid">
                    <!-- Fan Speed Control -->
                    <div class="panel-card">
                        <div class="panel-header">Fan Speed Control</div>
                        <div style="padding:16px;">
                            <div style="display:flex; justify-content:space-between; font-size:0.84rem; margin-bottom:8px;">
                                <span style="color:var(--text-muted);">Current Speed:</span>
                                <span id="dash-speed-label" style="font-weight:700; color:var(--accent-blue);">OFF</span>
                            </div>
                            <div class="btn-speed-group">
                                <button class="btn-speed active-0" id="dash-spd-0" onclick="setSpeed(0)">OFF</button>
                                <button class="btn-speed" id="dash-spd-1" onclick="setSpeed(1)">SLOW</button>
                                <button class="btn-speed" id="dash-spd-2" onclick="setSpeed(2)">MED</button>
                                <button class="btn-speed" id="dash-spd-3" onclick="setSpeed(3)">HIGH</button>
                            </div>
                        </div>
                    </div>

                    <!-- Room Air Valves -->
                    <div class="panel-card">
                        <div class="panel-header">Motorized Air Valves</div>
                        <div style="padding:12px;">
                            <div class="valve-item-row" id="valve-row-danylo">
                                <div>
                                    <div style="font-size:0.88rem; font-weight:700;">Danylo Room</div>
                                    <div style="font-size:0.75rem; color:var(--text-muted);" id="valve-desc-danylo">Valve Closed</div>
                                </div>
                                <label class="switch-toggle">
                                    <input type="checkbox" id="chk-valve-danylo" onchange="toggleValve('danylo', this.checked)">
                                    <span class="slider-round"></span>
                                </label>
                            </div>

                            <div class="valve-item-row" id="valve-row-anastasiia">
                                <div>
                                    <div style="font-size:0.88rem; font-weight:700;">Anastasiia Room</div>
                                    <div style="font-size:0.75rem; color:var(--text-muted);" id="valve-desc-anastasiia">Valve Closed</div>
                                </div>
                                <label class="switch-toggle">
                                    <input type="checkbox" id="chk-valve-anastasiia" onchange="toggleValve('anastasiia', this.checked)">
                                    <span class="slider-round"></span>
                                </label>
                            </div>

                            <div class="valve-item-row" id="valve-row-parents">
                                <div>
                                    <div style="font-size:0.88rem; font-weight:700;">Parents Room</div>
                                    <div style="font-size:0.75rem; color:var(--text-muted);" id="valve-desc-parents">Valve Closed</div>
                                </div>
                                <label class="switch-toggle">
                                    <input type="checkbox" id="chk-valve-parents" onchange="toggleValve('parents', this.checked)">
                                    <span class="slider-round"></span>
                                </label>
                            </div>

                            <div class="valve-item-row" id="valve-row-gym">
                                <div>
                                    <div style="font-size:0.88rem; font-weight:700;">Gym Room</div>
                                    <div style="font-size:0.75rem; color:var(--text-muted);" id="valve-desc-gym">Valve Closed</div>
                                </div>
                                <label class="switch-toggle">
                                    <input type="checkbox" id="chk-valve-gym" onchange="toggleValve('gym', this.checked)">
                                    <span class="slider-round"></span>
                                </label>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Sensor Nodes Overview -->
                <div class="panel-card">
                    <div class="panel-header">Room Sensors (Qingping Wi-Fi Direct + ESP-NOW)</div>
                    <div style="padding:16px;">
                        <div class="dashboard-grid" id="dash-sensors-grid"></div>
                    </div>
                </div>
            </div>

            <!-- ================= VIEW 3: SYSTEM LOG ================= -->
            <div id="view-syslog" style="display:none;">
                <div class="page-title-row">
                    <h2>System Diagnostics & Log</h2>
                </div>
                <div class="panel-card">
                    <div class="panel-header">Controller Metrics</div>
                    <table class="panel-table">
                        <tr><td class="col-label">System Uptime</td><td class="col-val" id="sys-uptime">--</td></tr>
                        <tr><td class="col-label">Free Heap Memory</td><td class="col-val" id="sys-heap">--</td></tr>
                        <tr><td class="col-label">Wi-Fi Channel</td><td class="col-val" id="sys-channel">--</td></tr>
                        <tr><td class="col-label">Network & AP Mode</td><td class="col-val" id="sys-ip-ap">--</td></tr>
                        <tr><td class="col-label">MAC Address</td><td class="col-val" id="sys-mac">--</td></tr>
                    </table>
                </div>
            </div>
        </main>
    </div>

                    <!-- ================= OTA FIRMWARE UPGRADE MODAL ================= -->
    <div class="modal-overlay" id="ota-modal">
        <div class="modal-window">
            <div class="modal-header">
                <span id="modal-title">Firmware Upgrade</span>
                <button class="modal-close" onclick="closeOtaModal()">&times;</button>
            </div>
            <div class="modal-body">
                <div class="modal-info-box">
                    <div class="modal-info-item">
                        <span class="label">Target Module</span>
                        <span class="val" id="modal-mod-name">Ventilation Master</span>
                    </div>
                    <div class="modal-info-item">
                        <span class="label">Installed Version</span>
                        <span class="val" id="modal-mod-cur-ver">v0.0.1</span>
                    </div>
                    <div class="modal-info-item">
                        <span class="label">Architecture</span>
                        <span class="val" id="modal-mod-model">ESP32</span>
                    </div>
                    <div class="modal-info-item">
                        <span class="label">Required Firmware</span>
                        <span class="val" id="modal-mod-expected">ventmaster_*.bin</span>
                    </div>
                </div>

                <div class="drop-zone" id="ota-drop-zone" onclick="document.getElementById('ota-bin-file').click()"
                     ondragover="event.preventDefault(); this.style.borderColor='var(--accent-blue)';"
                     ondragleave="event.preventDefault(); this.style.borderColor='#404f62';"
                     ondrop="onOtaFileDropped(event)">
                    <input type="file" id="ota-bin-file" accept=".bin" style="display:none;" onchange="onOtaFileChosen(this)">
                    <div style="font-size:1.8rem; margin-bottom:8px;">&#128190;</div>
                    <div style="font-size:0.9rem; font-weight:700; color:#fff;" id="file-drop-label">
                        Click or Drop .bin Firmware File Here
                    </div>
                    <div style="font-size:0.75rem; color:var(--text-muted); margin-top:4px;" id="file-size-info">
                        Select compiled firmware binary for this module
                    </div>
                </div>

                <div id="ota-validation-box" style="display:none; padding:10px 14px; border-radius:6px; font-size:0.8rem; font-weight:600; line-height:1.4;"></div>

                <div class="modal-progress" id="modal-progress-box">
                    <div style="display:flex; justify-content:space-between; font-size:0.78rem;">
                        <span id="ota-step-text" style="color:var(--accent-blue); font-weight:600;">Uploading firmware...</span>
                        <span id="ota-step-percent" style="color:#fff; font-weight:700;">0%</span>
                    </div>
                    <div class="progress-bar-track">
                        <div class="progress-bar-fill" id="ota-progress-bar"></div>
                    </div>
                    <div style="display:flex; justify-content:space-between; font-size:0.72rem; color:var(--text-muted);">
                        <span id="ota-bytes-text">0 / 0 KB</span>
                        <span id="ota-speed-text">-- KB/s</span>
                    </div>
                </div>

                <div style="font-size:0.75rem; color:#94a3b8; line-height:1.45;">
                    &#9888; <b>Notice:</b> Do not power off or disconnect the module while flashing. The module will reboot automatically when the update is complete.
                </div>
            </div>
            <div class="modal-footer">
                <button class="btn-modern" onclick="closeOtaModal()">Cancel</button>
                <button class="btn-modern" id="btn-modal-upload" style="background:linear-gradient(180deg, #10b981 0%, #059669 100%); border-color:#10b981; color:#fff;" onclick="startModuleUpload()" disabled>
                    Upload & Flash
                </button>
            </div>
        </div>
    </div>

    <!-- Application Script -->
    <script>
        let latestData = null;
        let selectedModule = null;
        let validatedOtaFile = null;
        let otaUploading = false;

        function toggleSidebar() {
            document.getElementById('main-sidebar').classList.toggle('open');
        }

        function switchView(viewName) {
            document.getElementById('view-admin').style.display = (viewName === 'admin') ? 'block' : 'none';
            document.getElementById('view-dashboard').style.display = (viewName === 'dashboard' || viewName === 'sensors') ? 'block' : 'none';
            document.getElementById('view-syslog').style.display = (viewName === 'syslog') ? 'block' : 'none';

            ['dashboard', 'sensors', 'admin', 'syslog'].forEach(id => {
                const el = document.getElementById('nav-' + id);
                if (el) {
                    if (id === viewName) el.classList.add('active');
                    else el.classList.remove('active');
                }
            });

            document.getElementById('main-sidebar').classList.remove('open');
        }

        function selectAdminTab(tab) {
            if (tab === 'firmware') {
                document.getElementById('tab-btn-fw').classList.add('active');
                document.getElementById('tab-btn-sys').classList.remove('active');
            } else if (tab === 'system') {
                switchView('syslog');
            }
        }

        async function fetchStatus() {
            if (otaUploading) return;
            try {
                const res = await fetch('/api/status');
                if (!res.ok) return;
                const data = await res.json();
                latestData = data;
                renderApp(data);
            } catch (err) {
                console.error("Status fetch error:", err);
            }
        }

        function renderApp(data) {
            // Top Badge
            const topBadge = document.getElementById('top-conn-text');
            if (topBadge) {
                topBadge.textContent = data.ap_active ? "SoftAP + Wi-Fi Active" : "Station Connected";
            }

            // Main Controller Status
            const ctrlVer = document.getElementById('ctrl-cur-ver');
            if (ctrlVer) ctrlVer.textContent = 'v' + (data.version || '0.0.1');

            const ctrlExtra = document.getElementById('ctrl-extra-info');
            if (ctrlExtra) {
                ctrlExtra.textContent = `(Partition: ${data.running_partition || 'ota_0'} | Build: ${data.build_date || 'OK'})`;
            }

            const ctrlIp = document.getElementById('ctrl-ip');
            if (ctrlIp) ctrlIp.textContent = data.local_ip || '192.168.51.100';

            // Sensor Modules List in Administration
            const tbody = document.getElementById('sensor-modules-tbody');
            if (tbody && data.modules) {
                let html = '';
                const nodeMods = data.modules.filter(m => m.role === 'sensor');

                if (nodeMods.length === 0) {
                    html = `<tr><td colspan="2" style="padding:14px; text-align:center; color:var(--text-muted);">
                        Waiting for wireless sensor nodes...</td></tr>`;
                } else {
                    nodeMods.forEach(m => {
                        const isQingping = (m.sensor_type === 'qingping');
                        const statusBadge = m.online 
                            ? `<span style="color:#4ade80; font-weight:700;">● Online</span> (${m.ip ? m.ip : (isQingping ? 'Wi-Fi MQTT' : 'ESP-NOW')}${m.mac ? ' | MAC: ' + m.mac : ''}, ${Math.round(m.ago_sec)}s ago)`
                            : `<span style="color:#f87171; font-weight:700;">● Offline</span>${m.mac ? ' (Bound MAC: ' + m.mac + ')' : ''}`;
                        
                        const actionHtml = isQingping
                            ? `<button class="btn-top-action" onclick="const el=document.getElementById('qp-mac-${m.id}'); if(el){el.scrollIntoView({behavior:'smooth',block:'center'}); el.focus();}">&#9881; Configure Qingping MAC</button>`
                            : `<button class="btn-update" onclick="openOtaModal('${m.id}')">&#11014; Update Firmware</button>`;

                        html += `
                        <tr>
                            <td class="col-label">
                                <div style="font-weight:700; color:#fff;">${m.name}</div>
                                <div style="font-size:0.75rem; color:#94a3b8;">${m.sensor_label || m.model}</div>
                                ${!isQingping ? `<div style="font-size:0.7rem; color:#64748b;"><code>ventslave_${m.id}_*.bin</code></div>` : ''}
                            </td>
                            <td class="col-val">
                                <div style="display:flex; flex-direction:column; gap:6px;">
                                    <div>${isQingping ? 'Model : <span class="fw-ver-tag">Qingping CGDN1</span>' : `Current Version : <span class="fw-ver-tag" onclick="openOtaModal('${m.id}')">v${m.version || '0.0.1'}</span>`}</div>
                                    <div style="font-size:0.8rem; color:var(--text-muted);">${statusBadge}</div>
                                    <div style="margin-top:2px;">
                                        ${actionHtml}
                                    </div>
                                </div>
                            </td>
                        </tr>`;
                    });
                }
                tbody.innerHTML = html;
            }

            // Dashboard Controls
            const modeBtn = document.getElementById('dash-mode-btn');
            if (modeBtn) {
                modeBtn.textContent = "Mode: " + (data.mode ? data.mode.toUpperCase() : "MANUAL");
                modeBtn.style.color = (data.mode === "auto") ? "#4ade80" : "#fbbf24";
            }

            const speed = data.speed || 0;
            const spdText = document.getElementById('dash-speed-label');
            const speedNames = ["OFF", "SLOW", "MEDIUM", "HIGH"];
            if (spdText) spdText.textContent = speedNames[speed] || "OFF";

            for (let i = 0; i <= 3; i++) {
                const b = document.getElementById(`dash-spd-${i}`);
                if (b) {
                    b.className = "btn-speed";
                    if (i === speed) b.classList.add(`active-${i}`);
                }
            }

            // Valves
            if (data.valves) {
                ['danylo', 'anastasiia', 'parents', 'gym'].forEach(r => {
                    const chk = document.getElementById(`chk-valve-${r}`);
                    const row = document.getElementById(`valve-row-${r}`);
                    const desc = document.getElementById(`valve-desc-${r}`);
                    const isOpen = !!data.valves[r];
                    if (chk) chk.checked = isOpen;
                    if (row) row.classList.toggle('open', isOpen);
                    if (desc) desc.textContent = isOpen ? "Valve Open (Air Flowing)" : "Valve Closed";
                });
            }

            // Sensors Grid
            const sensorsContainer = document.getElementById('dash-sensors-grid');
            if (sensorsContainer && data.sensors) {
                let sHtml = '';
                for (const [id, s] of Object.entries(data.sensors)) {
                    const isQingping = (s.sensor_type === 'qingping');
                    const srcTag = isQingping ? 'Qingping Wi-Fi' : 'ESP-NOW';
                    sHtml += `
                    <div class="sensor-card">
                        <div class="sensor-card-top">
                            <div>
                                <div style="font-weight:700; color:#fff;">${s.name}</div>
                                <div style="font-size:0.72rem; color:var(--accent-blue);">${s.sensor_label || srcTag}</div>
                            </div>
                            <span class="badge-status ${s.online ? 'badge-online' : 'badge-offline'}">
                                ${s.online ? 'ONLINE' : 'OFFLINE'}
                            </span>
                        </div>
                        <div class="sensor-metrics-grid">
                            <div>
                                <div class="sensor-metric-val">${s.online ? s.temp.toFixed(1) + '°C' : '--'}</div>
                                <div class="sensor-metric-label">Temperature</div>
                            </div>
                            <div>
                                <div class="sensor-metric-val" style="color:#4ade80;">${s.online ? s.hum.toFixed(1) + '%' : '--'}</div>
                                <div class="sensor-metric-label">Humidity</div>
                            </div>
                            <div>
                                <div class="sensor-metric-val" style="color:#fbbf24;">${(s.online && s.co2 > 0) ? s.co2 + ' ppm' : '--'}</div>
                                <div class="sensor-metric-label">CO₂ Level</div>
                            </div>
                            <div>
                                <div class="sensor-metric-val" style="color:#c084fc;">${s.online ? s.pm25 + ' µg/m³' : '--'}</div>
                                <div class="sensor-metric-label">PM2.5${(s.online && s.pm10 > 0) ? ' (PM10: ' + s.pm10 + ')' : ''}</div>
                            </div>
                        </div>
                        <div style="font-size:0.75rem; color:var(--text-muted); display:flex; justify-content:space-between;">
                            <span>${isQingping ? (s.mac ? 'MAC: ' + s.mac : 'Unbound MAC') : 'FW: v' + (s.version || '0.0.1')}</span>
                            <span>${s.online ? Math.round(s.ago_sec) + 's ago' : 'Offline'}</span>
                        </div>
                    </div>`;
                }
                sensorsContainer.innerHTML = sHtml;
            }

            // Qingping Direct Wi-Fi Binding & Discovery Section
            const qpBrokerIp = document.getElementById('qp-broker-ip');
            if (qpBrokerIp && data.local_ip) {
                qpBrokerIp.textContent = data.local_ip;
            }
            const qpClientCnt = document.getElementById('qp-client-count');
            if (qpClientCnt) {
                qpClientCnt.textContent = (data.qingping_clients !== undefined) ? data.qingping_clients : 0;
            }
            if (data.sensors) {
                ['danylo', 'anastasiia', 'parents'].forEach(r => {
                    const s = data.sensors[r];
                    const curEl = document.getElementById(`qp-cur-${r}`);
                    const inpEl = document.getElementById(`qp-mac-${r}`);
                    if (curEl && s) {
                        curEl.innerHTML = s.assigned_mac
                            ? `<span style="color:#4ade80;">Bound: <code>${s.mac}</code></span>`
                            : `<span style="color:#f59e0b;">Not bound yet</span>`;
                    }
                    if (inpEl && s && s.mac && document.activeElement !== inpEl && !inpEl.value) {
                        inpEl.value = s.mac;
                    }
                });
            }
            const qpListEl = document.getElementById('qp-discovered-list');
            if (qpListEl && data.discovered_qingpings) {
                if (data.discovered_qingpings.length === 0) {
                    qpListEl.innerHTML = `<div style="padding:10px; background:#161c24; border:1px solid #283340; border-radius:6px;">
                        Waiting for Qingping monitors to report on TCP port <code>${data.qingping_broker_port || 1883}</code> (or MQTT <code>qingping/+/up</code>)...
                    </div>`;
                } else {
                    let dHtml = '<div style="display:flex; flex-direction:column; gap:8px;">';
                    data.discovered_qingpings.forEach(dq => {
                        const boundBadge = dq.bound_room
                            ? `<span style="color:#4ade80; font-weight:700;">Bound &rarr; ${dq.bound_room.toUpperCase()}</span>`
                            : `<span style="color:#fbbf24; font-weight:700;">Unassigned</span>`;
                        const metricsText = dq.has_metrics
                            ? `CO₂: <b style="color:#fbbf24;">${dq.co2} ppm</b> | Temp: <b style="color:#38bdf8;">${dq.temp.toFixed(1)}°C</b> | Hum: <b style="color:#4ade80;">${dq.hum.toFixed(1)}%</b> | PM2.5: <b>${dq.pm25} µg/m³</b> | PM10: <b>${dq.pm10} µg/m³</b>`
                            : `Connected (waiting for sensor payload...)`;
                        dHtml += `
                        <div style="background:#161c24; border:1px solid #283340; border-radius:6px; padding:10px 14px; display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:10px;">
                            <div>
                                <div style="font-weight:700; color:#fff;">
                                    Qingping <code>${dq.formatted_mac}</code> (${dq.ip || 'Wi-Fi'}, ${Math.round(dq.ago_sec)}s ago) &bull; ${boundBadge}
                                </div>
                                <div style="font-size:0.78rem; color:#cbd5e1; margin-top:3px;">${metricsText}</div>
                            </div>
                            <div style="display:flex; gap:6px; flex-wrap:wrap;">
                                <button class="btn-top-action" onclick="bindQingping('${dq.mac}', 'danylo')">Bind &rarr; Danylo (Stue)</button>
                                <button class="btn-top-action" onclick="bindQingping('${dq.mac}', 'anastasiia')">Bind &rarr; Anastasiia (Kontor)</button>
                                <button class="btn-top-action" onclick="bindQingping('${dq.mac}', 'parents')">Bind &rarr; Parents (Foreldre)</button>
                            </div>
                        </div>`;
                    });
                    dHtml += '</div>';
                    qpListEl.innerHTML = dHtml;
                }
            }
            const qpLogsEl = document.getElementById('qp-broker-logs');
            if (qpLogsEl && data.qingping_logs) {
                qpLogsEl.textContent = data.qingping_logs.length > 0
                    ? data.qingping_logs.join('\n')
                    : 'No TCP connections received on port 1883 yet.';
            }

            // System Diagnostics
            const uptimeEl = document.getElementById('sys-uptime');
            if (uptimeEl) {
                const u = data.uptime_sec || 0;
                const hrs = Math.floor(u / 3600);
                const mins = Math.floor((u % 3600) / 60);
                const secs = u % 60;
                uptimeEl.textContent = `${hrs} hours, ${mins} minutes, ${secs} seconds`;
            }
            const heapEl = document.getElementById('sys-heap');
            if (heapEl) heapEl.textContent = `${data.free_heap || 0} bytes free`;
            const chEl = document.getElementById('sys-channel');
            if (chEl) chEl.textContent = `Channel ${data.wifi_channel || 1}`;
            const ipApEl = document.getElementById('sys-ip-ap');
            if (ipApEl) ipApEl.textContent = `IP: ${data.local_ip || '--'} | AP Active: ${data.ap_active ? 'YES' : 'NO'}`;
            const macEl = document.getElementById('sys-mac');
            if (macEl) macEl.textContent = data.mac || '--';
        }

        // ================= OTA UPGRADE HANDLER =================
        function readNullTerminatedAscii(bytes, start, maxLen) {
            let out = '';
            for (let i = 0; i < maxLen && (start + i) < bytes.length; i++) {
                const c = bytes[start + i];
                if (c === 0) break;
                out += String.fromCharCode(c);
            }
            return out.trim();
        }

        function setOtaValidationStatus(isValid, message) {
            const box = document.getElementById('ota-validation-box');
            const uploadBtn = document.getElementById('btn-modal-upload');
            if (!box) return;
            box.style.display = 'block';
            if (isValid) {
                box.style.background = 'rgba(34, 197, 94, 0.15)';
                box.style.border = '1px solid rgba(34, 197, 94, 0.45)';
                box.style.color = '#4ade80';
                box.textContent = message;
                uploadBtn.disabled = false;
            } else {
                box.style.background = 'rgba(239, 68, 68, 0.15)';
                box.style.border = '1px solid rgba(239, 68, 68, 0.5)';
                box.style.color = '#f87171';
                box.textContent = message;
                uploadBtn.disabled = true;
            }
        }

        async function validateFirmwareFile(file, mod) {
            const fnLower = file.name.toLowerCase();
            if (!fnLower.endsWith('.bin')) {
                return { ok: false, error: '❌ Invalid file extension: Please select a compiled .bin firmware file.' };
            }

            const isMasterTarget = (mod.id === 'main' || mod.role === 'controller');
            const expectedRole = isMasterTarget ? 'master' : 'slave';
            const expectedTargetId = isMasterTarget ? 'master' : mod.id.toLowerCase();
            const expectedPrefix = isMasterTarget ? 'ventmaster_' : `ventslave_${expectedTargetId}_`;
            const isMigrateParentsToGym = (!isMasterTarget && fnLower.startsWith('ventslave_migrate_parents_to_gym_') && (expectedTargetId === 'parents' || expectedTargetId === 'gym'));

            // 1. Filename prefix check
            if (isMasterTarget && fnLower.startsWith('ventslave')) {
                return {
                    ok: false,
                    error: `❌ Firmware Mismatch: "${file.name}" is a Slave Sensor firmware file and cannot be flashed to Ventilation Master.`
                };
            }
            if (!isMasterTarget && fnLower.startsWith('ventmaster')) {
                return {
                    ok: false,
                    error: `❌ Firmware Mismatch: "${file.name}" is a Master Controller firmware file and cannot be flashed to ${mod.name}.`
                };
            }
            if (!isMasterTarget && fnLower.startsWith('ventslave_') && !fnLower.startsWith(expectedPrefix) && !isMigrateParentsToGym) {
                return {
                    ok: false,
                    error: `❌ Room Mismatch: "${file.name}" does not match ${mod.name} (expected "${expectedPrefix}*.bin").`
                };
            }

            // 2. Binary header & embedded VentFwTargetDesc check (first 352 bytes)
            if (file.size < 352) {
                return { ok: false, error: '❌ Invalid binary: File is smaller than minimum ESP32 header size (352 bytes).' };
            }

            const buf = await file.slice(0, 352).arrayBuffer();
            const bytes = new Uint8Array(buf);

            if (bytes[0] !== 0xE9) {
                return { ok: false, error: '❌ Invalid binary: Missing ESP32 image magic byte (0xE9).' };
            }

            const chipId = bytes[12] | (bytes[13] << 8);
            if (isMasterTarget && chipId === 0x0005) {
                return {
                    ok: false,
                    error: '❌ Architecture Mismatch: Binary is compiled for ESP32-C3 (Slave Sensor), but Master requires ESP32.'
                };
            }
            if (!isMasterTarget && chipId === 0x0000) {
                return {
                    ok: false,
                    error: `❌ Architecture Mismatch: Binary is compiled for ESP32 (Master Controller), but ${mod.name} requires ESP32-C3.`
                };
            }

            // Check VentFwTargetDesc at offset 288
            const magic = readNullTerminatedAscii(bytes, 288, 16);
            if (magic !== 'VENT_FW_DESC_V1') {
                return {
                    ok: false,
                    error: '❌ Missing Target Signature: Binary does not contain VENT_FW_DESC_V1 identity descriptor. Rebuild with build.ps1.'
                };
            }

            const binRole = readNullTerminatedAscii(bytes, 304, 16).toLowerCase();
            const binTargetId = readNullTerminatedAscii(bytes, 320, 16).toLowerCase();
            const binVersion = readNullTerminatedAscii(bytes, 336, 16);

            if (binRole !== expectedRole || (binTargetId !== expectedTargetId && !isMigrateParentsToGym)) {
                return {
                    ok: false,
                    error: `❌ Target Mismatch: Binary is compiled for "${binRole}:${binTargetId}", cannot flash onto "${expectedRole}:${expectedTargetId}" (${mod.name}).`
                };
            }

            return {
                ok: true,
                role: binRole,
                targetId: binTargetId,
                version: binVersion,
                chip: chipId === 0x0000 ? 'ESP32' : 'ESP32-C3'
            };
        }

        function openOtaModal(moduleId) {
            let mod = null;
            if (moduleId === 'main') {
                mod = latestData ? latestData.modules.find(m => m.id === 'main') : null;
                if (!mod) {
                    mod = {
                        id: 'main',
                        name: 'Ventilation Master',
                        model: 'ESP32 Dev Module',
                        role: 'controller',
                        version: latestData ? latestData.version : '0.0.1',
                        update_url: '/update'
                    };
                }
            } else if (latestData && latestData.modules) {
                mod = latestData.modules.find(m => m.id === moduleId);
            }

            if (!mod) return;
            selectedModule = mod;
            validatedOtaFile = null;

            const isMaster = (mod.id === 'main' || mod.role === 'controller');
            const expectedFilePattern = isMaster ? 'ventmaster_*.bin' : `ventslave_${mod.id}_*.bin`;

            document.getElementById('modal-title').textContent = `Firmware Upgrade - ${mod.name}`;
            document.getElementById('modal-mod-name').textContent = mod.name;
            document.getElementById('modal-mod-cur-ver').textContent = 'v' + (mod.version || '0.0.1');
            document.getElementById('modal-mod-model').textContent = mod.model || 'ESP32';
            document.getElementById('modal-mod-expected').textContent = expectedFilePattern;

            document.getElementById('ota-bin-file').value = '';
            document.getElementById('file-drop-label').textContent = 'Click or Drop .bin Firmware File Here';
            document.getElementById('file-size-info').textContent = `Required binary target: ${isMaster ? 'master:master' : 'slave:' + mod.id} (${expectedFilePattern})`;
            document.getElementById('ota-validation-box').style.display = 'none';
            document.getElementById('modal-progress-box').style.display = 'none';
            document.getElementById('btn-modal-upload').disabled = true;

            document.getElementById('ota-modal').style.display = 'flex';
        }

        function closeOtaModal() {
            document.getElementById('ota-modal').style.display = 'none';
        }

        async function handleSelectedOtaFile(file) {
            if (!file || !selectedModule) return;
            validatedOtaFile = null;
            const kb = (file.size / 1024).toFixed(1);
            document.getElementById('file-drop-label').textContent = file.name;
            document.getElementById('file-size-info').textContent = `Size: ${kb} KB - Verifying target signature...`;
            document.getElementById('btn-modal-upload').disabled = true;

            try {
                const check = await validateFirmwareFile(file, selectedModule);
                if (check.ok) {
                    validatedOtaFile = file;
                    document.getElementById('file-size-info').textContent = `Size: ${kb} KB - Verified for ${selectedModule.name}`;
                    setOtaValidationStatus(true, `✅ Target Verified: ${check.role}:${check.targetId} (v${check.version || '?'}, ${check.chip}) — Ready to flash`);
                } else {
                    document.getElementById('file-size-info').textContent = `Size: ${kb} KB - Rejected`;
                    setOtaValidationStatus(false, check.error);
                }
            } catch (err) {
                setOtaValidationStatus(false, '❌ Error reading firmware file: ' + err.message);
            }
        }

        function onOtaFileChosen(input) {
            if (input.files && input.files.length > 0) {
                handleSelectedOtaFile(input.files[0]);
            }
        }

        function onOtaFileDropped(event) {
            event.preventDefault();
            const dz = document.getElementById('ota-drop-zone');
            if (dz) dz.style.borderColor = '#404f62';
            if (event.dataTransfer && event.dataTransfer.files && event.dataTransfer.files.length > 0) {
                handleSelectedOtaFile(event.dataTransfer.files[0]);
            }
        }

        function startModuleUpload() {
            const file = validatedOtaFile;
            if (!file || !selectedModule) return;
            const uploadBtn = document.getElementById('btn-modal-upload');
            const progBox = document.getElementById('modal-progress-box');
            const progBar = document.getElementById('ota-progress-bar');
            const progPercent = document.getElementById('ota-step-percent');
            const progStep = document.getElementById('ota-step-text');
            const bytesText = document.getElementById('ota-bytes-text');
            const speedText = document.getElementById('ota-speed-text');

            otaUploading = true;
            uploadBtn.disabled = true;
            progBox.style.display = 'flex';
            progBar.style.width = '0%';
            progPercent.textContent = '0%';
            progStep.style.color = 'var(--accent-blue)';
            progStep.textContent = `Uploading firmware to ${selectedModule.name}...`;

            const formData = new FormData();
            formData.append('update', file);

            let uploadUrl = '/update';
            if (selectedModule.ip && selectedModule.ip !== '0.0.0.0') {
                uploadUrl = `http://${selectedModule.ip}/update`;
            }

            const xhr = new XMLHttpRequest();
            const startTime = Date.now();

            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    progBar.style.width = percent + '%';
                    progPercent.textContent = percent + '%';
                    
                    const curKb = Math.round(e.loaded / 1024);
                    const totKb = Math.round(e.total / 1024);
                    bytesText.textContent = `${curKb} / ${totKb} KB`;

                    const elapsedSec = (Date.now() - startTime) / 1000;
                    if (elapsedSec > 0.5) {
                        const kbps = Math.round(curKb / elapsedSec);
                        speedText.textContent = `${kbps} KB/s`;
                    }

                    if (percent >= 100) {
                        progStep.textContent = "Writing flash memory...";
                        progPercent.textContent = "⏳";
                    }
                }
            });

            xhr.onload = function() {
                let res = {};
                try { res = JSON.parse(xhr.responseText); } catch(e) { res = { message: xhr.responseText }; }

                if (xhr.status === 200) {
                    progBar.style.width = '100%';
                    progPercent.textContent = '100%';
                    progStep.style.color = '#4ade80';
                    progStep.textContent = `✅ Flash Complete! ${selectedModule.name} is rebooting...`;

                    let countdown = 10;
                    const interval = setInterval(() => {
                        countdown--;
                        speedText.textContent = `Reconnecting: ${countdown}s`;
                        if (countdown <= 0) {
                            clearInterval(interval);
                            otaUploading = false;
                            closeOtaModal();
                            window.location.reload();
                        }
                    }, 1000);
                } else {
                    otaUploading = false;
                    progStep.style.color = '#ef4444';
                    progStep.textContent = "❌ Flash Error: " + (res.message || "Failed to flash firmware.");
                    uploadBtn.disabled = false;
                }
            };

            xhr.onerror = function() {
                otaUploading = false;
                progStep.style.color = '#ef4444';
                progStep.textContent = `❌ Network transfer failed to ${uploadUrl}`;
                uploadBtn.disabled = false;
            };

            xhr.open('POST', uploadUrl);
            xhr.send(formData);
        }

        // Dashboard control APIs
        async function setSpeed(s) {
            try {
                await fetch(`/api/speed?val=${s}`, { method: 'POST' });
                fetchStatus();
            } catch(e) { console.error(e); }
        }

        async function toggleValve(r, state) {
            try {
                await fetch(`/api/valve?room=${r}&state=${state ? 1 : 0}`, { method: 'POST' });
                fetchStatus();
            } catch(e) { console.error(e); }
        }

        async function toggleMode() {
            const next = (latestData && latestData.mode === 'auto') ? 'manual' : 'auto';
            try {
                await fetch(`/api/mode?val=${next}`, { method: 'POST' });
                fetchStatus();
            } catch(e) { console.error(e); }
        }

        async function bindQingping(mac, room) {
            try {
                const res = await fetch(`/api/qingping/bind?room=${encodeURIComponent(room)}&mac=${encodeURIComponent(mac)}`, { method: 'POST' });
                if (res.ok) {
                    const inp = document.getElementById(`qp-mac-${room}`);
                    if (inp) inp.value = mac;
                    fetchStatus();
                } else {
                    alert("Failed to bind Qingping MAC. Make sure it is a valid 12-hex MAC address.");
                }
            } catch (e) {
                console.error(e);
            }
        }

        async function saveQingpingMac(room) {
            const inp = document.getElementById(`qp-mac-${room}`);
            const mac = inp ? inp.value.trim() : '';
            await bindQingping(mac, room);
        }

        async function restartController() {
            if (!confirm("Are you sure you want to restart the Ventilation Controller?")) return;
            try {
                await fetch('/api/restart', { method: 'POST' });
                alert("Controller restart initiated. Reconnecting in 5 seconds...");
                setTimeout(() => window.location.reload(), 5000);
            } catch(e) { console.error(e); }
        }

        // Start status polling
        fetchStatus();
        setInterval(fetchStatus, 2000);
    </script>
</body>
</html>
)rawliteral";
