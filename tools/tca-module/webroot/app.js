/* TCA Cycle Tuner - KernelSU/ReSukiSU 模块 WebUI（Material Design 3） */
(function () {
  'use strict';

  var DEFAULT_DIR = '/data/adb/modules/tca-cycle-tuner';
  var CONF = '/data/adb/tca-webui.conf';
  var LF = String.fromCharCode(10);
  var SUN = 'M12 9c1.65 0 3 1.35 3 3s-1.35 3-3 3-3-1.35-3-3 1.35-3 3-3m0-2c-2.76 0-5 2.24-5 5s2.24 5 5 5 5-2.24 5-5-2.24-5-5-5zM2 13h2c.55 0 1-.45 1-1s-.45-1-1-1H2c-.55 0-1 .45-1 1s.45 1 1 1zm18 0h2c.55 0 1-.45 1-1s-.45-1-1-1h-2c-.55 0-1 .45-1 1s.45 1 1 1zM11 2v2c0 .55.45 1 1 1s1-.45 1-1V2c0-.55-.45-1-1-1s-1 .45-1 1zm0 18v2c0 .55.45 1 1 1s1-.45 1-1v-2c0-.55-.45-1-1-1s-1 .45-1 1zM5.99 4.58a.996.996 0 0 0-1.41 0 .996.996 0 0 0 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0s.39-1.03 0-1.41L5.99 4.58zm12.37 12.37a.996.996 0 0 0-1.41 0 .996.996 0 0 0 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0a.996.996 0 0 0 0-1.41l-1.06-1.06zm1.06-10.96a.996.996 0 0 0 0-1.41.996.996 0 0 0-1.41 0l-1.06 1.06c-.39.39-.39 1.03 0 1.41s1.03.39 1.41 0l1.06-1.06zM7.05 18.36a.996.996 0 0 0 0-1.41.996.996 0 0 0-1.41 0l-1.06 1.06c-.39.39-.39 1.03 0 1.41s1.03.39 1.41 0l1.06-1.06z';
  var MOON = 'M12 3c-4.97 0-9 4.03-9 9s4.03 9 9 9 9-4.03 9-9c0-.46-.04-.92-.1-1.36-.98 1.37-2.58 2.26-4.4 2.26-3.03 0-5.5-2.47-5.5-5.5 0-1.82.89-3.42 2.26-4.4-.44-.06-.9-.1-1.36-.1z';

  var MODDIR = DEFAULT_DIR;
  var bridge = !!(window.ksu && typeof window.ksu.exec === 'function');
  var logEl = document.getElementById('log');

  function $(id) { return document.getElementById(id); }
  function shq(s) { return "'" + String(s).replace(/'/g, "'\\''") + "'"; }
  function val(id) { var e = $(id); return e ? String(e.value).trim() : ''; }

  function log(text) {
    if (!logEl) return;
    logEl.textContent += text + LF;
    logEl.scrollTop = logEl.scrollHeight;
  }

  function showOut(id, text) {
    var el = $(id);
    if (!el) return;
    el.textContent = text;
    el.classList.remove('hidden');
  }

  var snackTimer = null;
  function toast(msg, isErr) {
    var s = $('snackbar');
    if (!s) return;
    s.textContent = msg;
    s.style.borderLeft = isErr ? '4px solid var(--md-error)' : 'none';
    s.classList.add('show');
    if (snackTimer) window.clearTimeout(snackTimer);
    snackTimer = window.setTimeout(function () { s.classList.remove('show'); }, 2600);
  }

  var pending = 0;
  function progressOn() { pending++; var p = $('progress'); if (p) p.classList.add('on'); }
  function progressOff() { pending = Math.max(0, pending - 1); if (pending === 0) { var p = $('progress'); if (p) p.classList.remove('on'); } }

  function ksuExec(cmd) {
    return new Promise(function (resolve, reject) {
      if (!bridge) { reject(new Error('没有 KernelSU WebUI 桥接')); return; }
      var name = '__tca_cb_' + Date.now() + '_' + Math.floor(Math.random() * 1000000);
      var settled = false;
      window[name] = function (errno, stdout, stderr) {
        settled = true;
        try { delete window[name]; } catch (e) {}
        resolve({ errno: errno, stdout: stdout || '', stderr: stderr || '' });
      };
      try {
        window.ksu.exec(cmd, '{}', name);
      } catch (e) {
        try { delete window[name]; } catch (e2) {}
        reject(e);
        return;
      }
      window.setTimeout(function () {
        if (settled) return;
        try { delete window[name]; } catch (e3) {}
        reject(new Error('命令超时'));
      }, 90000);
    });
  }

  var queue = Promise.resolve();

  function markSegmented(cmd) {
    if (cmd.indexOf('sched ') !== 0) return;
    var seg = $('sched-seg');
    if (!seg) return;
    Array.prototype.forEach.call(seg.querySelectorAll('button'), function (b) {
      b.classList.toggle('selected', b.getAttribute('data-cmd') === cmd);
    });
  }

  function run(args, target) {
    var full = 'sh ' + shq(MODDIR + '/tca-tune.sh') + ' ' + args;
    queue = queue.then(function () {
      log('$ tca-tune.sh ' + args);
      progressOn();
      return ksuExec(full).then(function (r) {
        var out = (r.stdout || '') + (r.stderr || '');
        if (out) log(out);
        if (Number(r.errno) !== 0) {
          log('[exit ' + r.errno + ']');
          toast('命令失败，请查看输出（exit ' + r.errno + '）', true);
        } else {
          markSegmented(args);
        }
        if (target) showOut(target, out || '(无输出)');
      }).catch(function (e) {
        log('[失败] ' + e.message);
        if (target) showOut(target, '[失败] ' + e.message);
        toast('执行失败：' + e.message, true);
      }).then(function () { progressOff(); });
    });
    return queue;
  }

  function targetFor(cmd) {
    var c = String(cmd).split(' ')[0];
    if (c === 'charge') return 'out-charge';
    if (c === 'touch') return 'out-touch';
    if (c === 'mem') return 'out-mem';
    if (c === 'gc') return 'out-mem';
    if (c === 'audio') return 'out-audio';
    if (c === 'sched') return 'out-sched';
    if (c === 'net' || c === 'io') return 'out-netio';
    if (c === 'fas') return 'out-fas';
    if (c === 'show') return 'status';
    return null;
  }

  function resolveModDir() {
    if (!bridge) return Promise.resolve(DEFAULT_DIR);
    var cands = [];
    try {
      var raw = window.ksu.moduleInfo ? window.ksu.moduleInfo() : '';
      if (raw) {
        var info = JSON.parse(raw);
        if (info.moduleDir) cands.push(info.moduleDir);
        if (info.moduleId) cands.push('/data/adb/modules/' + info.moduleId);
      }
    } catch (e) {}
    cands.push(DEFAULT_DIR, '/data/adb/modules_update/tca-cycle-tuner');
    var i = 0;
    function next() {
      if (i >= cands.length) return Promise.resolve(DEFAULT_DIR);
      var c = cands[i++];
      return ksuExec('test -f ' + shq(c + '/tca-tune.sh') + ' && echo TCAOK').then(function (r) {
        return (r.stdout.indexOf('TCAOK') >= 0) ? c : next();
      }).catch(function () { return next(); });
    }
    return next();
  }

  function readBoot() {
    return ksuExec('cat ' + CONF + ' 2>/dev/null').then(function (r) {
      showOut('boot-view', r.stdout || '(空：开机不应用)');
    }).catch(function () {});
  }

  function saveBoot() {
    var lines = [];
    var p = val('b-profile');
    if (p) lines.push(p);
    if ($('b-limit').checked) lines.push('charge limit 80');
    if ($('b-fas') && $('b-fas').checked) {
      lines.push('fas preset ' + (val('b-fas-preset') || 'smooth'));
      lines.push('fas on');
    }
    if ($('b-audio').checked) lines.push('audio idle on');
    var text = lines.join(LF);
    var b64 = '';
    try { b64 = btoa(unescape(encodeURIComponent(text))); } catch (e) { b64 = ''; }
    progressOn();
    return ksuExec("echo '" + b64 + "' | base64 -d > " + CONF).then(function (r) {
      if (Number(r.errno) !== 0) throw new Error(r.stderr || '写入失败');
      showOut('boot-view', text || '(空：开机不应用)');
      log('[开机] 已保存' + (lines.length ? '：' + lines.join(' / ') : '（空）'));
      toast(lines.length ? '已保存开机档位' : '已清空开机档位');
    }).catch(function (e) { toast('保存失败：' + e.message, true); }).then(function () { progressOff(); });
  }

  function clearBoot() {
    progressOn();
    return ksuExec('rm -f ' + CONF).then(function () {
      showOut('boot-view', '(空：开机不应用)');
      toast('已清除开机档位');
    }).catch(function () {}).then(function () { progressOff(); });
  }

  function refresh() {
    return run('show', 'status').then(function () {
      return run('charge show', 'charge-status');
    });
  }

  function moveIndicator(btn) {
    var ind = $('tab-indicator');
    if (!ind || !btn) return;
    ind.style.width = btn.offsetWidth + 'px';
    ind.style.transform = 'translateX(' + btn.offsetLeft + 'px)';
  }

  /* FAS 实时状态：静默轮询（不经 run()，不刷日志） */
  var fasTimer = null;
  function fasPollOnce() {
    if (!bridge) return;
    ksuExec('sh ' + shq(MODDIR + '/tca-tune.sh') + ' fas live').then(function (r) {
      var el = $('fas-live');
      if (el && r.stdout) el.textContent = r.stdout.trim();
    }).catch(function () {});
  }
  function fasPollStop() {
    if (fasTimer) { window.clearInterval(fasTimer); fasTimer = null; }
    var b = $('btn-fas-live'); if (b) b.textContent = '开始 1s 轮询';
  }

  function selectTab(btn) {
    Array.prototype.forEach.call(document.querySelectorAll('.tab'), function (x) {
      x.classList.toggle('active', x === btn);
    });
    var id = 'panel-' + btn.getAttribute('data-tab');
    Array.prototype.forEach.call(document.querySelectorAll('.panel'), function (p) {
      p.classList.toggle('active', p.id === id);
    });
    moveIndicator(btn);
    if (btn.getAttribute('data-tab') === 'fas') fasPollOnce(); else fasPollStop();
    try { btn.scrollIntoView({ inline: 'center', block: 'nearest' }); } catch (e) {}
  }

  function initTheme() {
    var t = null;
    try { t = localStorage.getItem('tca-theme'); } catch (e) {}
    if (t !== 'dark' && t !== 'light') {
      var dark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
      t = dark ? 'dark' : 'light';
    }
    applyTheme(t);
  }

  function applyTheme(t) {
    document.documentElement.setAttribute('data-theme', t);
    var p = $('theme-icon');
    if (p) p.setAttribute('d', t === 'dark' ? SUN : MOON);
    try { localStorage.setItem('tca-theme', t); } catch (e) {}
    applyDyn();
  }

  function attachRipple(ev) {
    var el = ev.target && ev.target.closest ? ev.target.closest('.md-ripple') : null;
    if (!el) return;
    var r = el.getBoundingClientRect();
    if (!r.width || !r.height) return;
    var d = Math.max(r.width, r.height) * 1.2;
    var s = document.createElement('span');
    s.className = 'ripple';
    s.style.width = d + 'px';
    s.style.height = d + 'px';
    s.style.left = (ev.clientX - r.left - d / 2) + 'px';
    s.style.top = (ev.clientY - r.top - d / 2) + 'px';
    el.appendChild(s);
    window.setTimeout(function () { if (s.parentNode) s.parentNode.removeChild(s); }, 520);
  }

  var DYNAMIC = {
    'touch-freq': function () { return 'touch freq ' + val('t-freq'); },
    'touch-ms': function () { return 'touch ms ' + val('t-ms'); },
    'touch-pct': function () { return 'touch uclamp_pct ' + val('t-pct'); },
    'touch-cpu': function () { return 'touch cpu ' + val('t-cpu'); },
    'touch-idle': function () { return 'touch idle ' + val('t-idle'); },
    'charge-power': function () { return 'charge power ' + val('c-mw'); },
    'charge-current': function () { return 'charge current ' + val('c-ma'); },
    'charge-limit': function () { return 'charge limit ' + val('c-pct'); },
    'fas-fps': function () { return 'fas set target_fps ' + val('f-fps'); },
    'fas-margin': function () { return 'fas set margin_us ' + val('f-margin'); },
    'fas-headroom': function () { return 'fas set headroom_pct ' + val('f-headroom'); },
    'fas-gain': function () { return 'fas set gain_khz_per_ms ' + val('f-gain'); },
    'fas-smooth': function () { return 'fas set smooth_shift ' + val('f-smooth'); },
    'fas-minpct': function () { return 'fas set min_pct ' + val('f-minpct'); },
    'fas-minkhz': function () { return 'fas set min_khz ' + val('f-minkhz'); },
    'fas-maxkhz': function () { return 'fas set max_khz ' + val('f-maxkhz'); }
  };

  function normalize(cmd) {
    return cmd.split(' ').filter(function (x) { return x !== ''; }).join(' ');
  }

  function bind() {
    document.addEventListener('pointerdown', attachRipple, true);

    document.addEventListener('click', function (ev) {
      var t = ev.target;
      if (!t || !t.closest) return;

      var tab = t.closest('.tab');
      if (tab) { selectTab(tab); return; }

      var dyn = t.closest('[data-run]');
      if (dyn) {
        var f = DYNAMIC[dyn.getAttribute('data-run')];
        if (f) {
          var cmd = normalize(f());
          var parts = cmd.split(' ');
          var lastv = parts[parts.length - 1];
          if (!lastv || lastv === 'undefined' || lastv === 'NaN') { toast('请先填数值', true); return; }
          run(cmd, targetFor(cmd));
        }
        return;
      }

      var btn = t.closest('[data-cmd]');
      if (btn) {
        var c = btn.getAttribute('data-cmd');
        run(c, btn.getAttribute('data-target') || targetFor(c));
      }
    });

    var map = { 't-boost': 'touch boost ', 't-uclamp': 'touch uclamp ', 't-aff': 'touch affinity ' };
    Object.keys(map).forEach(function (id) {
      var e = $(id);
      if (!e) return;
      e.addEventListener('change', function () { run(map[id] + (e.checked ? '1' : '0'), 'out-touch'); });
    });

    var fon = $('fas-on');
    if (fon) fon.addEventListener('change', function () { run('fas ' + (fon.checked ? 'on' : 'off'), 'out-fas').then(fasPollOnce); });
    var fad = $('fas-adaptive');
    if (fad) fad.addEventListener('change', function () { run('fas set adaptive ' + (fad.checked ? '1' : '0'), 'out-fas'); });
    var flb = $('btn-fas-live');
    if (flb) flb.addEventListener('click', function () {
      if (fasTimer) { fasPollStop(); return; }
      flb.textContent = '停止轮询';
      fasPollOnce();
      fasTimer = window.setInterval(fasPollOnce, 1000);
    });

    var rf = $('btn-refresh'); if (rf) rf.addEventListener('click', function () { refresh(); });
    var cl = $('btn-clear'); if (cl) cl.addEventListener('click', function () { if (logEl) logEl.textContent = ''; });
    var sb = $('btn-save-boot'); if (sb) sb.addEventListener('click', saveBoot);
    var cb = $('btn-clear-boot'); if (cb) cb.addEventListener('click', clearBoot);
    var th = $('btn-theme'); if (th) th.addEventListener('click', function () {
      var cur = document.documentElement.getAttribute('data-theme');
      applyTheme(cur === 'dark' ? 'light' : 'dark');
    });
    var dcb = $('dyn-on');
    if (dcb) dcb.addEventListener('change', function () {
      if (dcb.checked) { enableDyn(false); }
      else { dyn.on = false; applyDyn(); saveDyn(); updateSeedLabel(); }
    });
    var dvs = $('dyn-variant');
    if (dvs) dvs.addEventListener('change', function () {
      dyn.variant = dvs.value || 'tonal';
      if (dyn.seed) dyn.cache = buildDyn(dyn.seed, dyn.variant);
      applyDyn(); saveDyn();
    });
    var drf = $('btn-dyn-refresh');
    if (drf) drf.addEventListener('click', function () { enableDyn(true); });

    var sheet = $('sheet'); var toggle = $('sheet-toggle');
    if (sheet && toggle) toggle.addEventListener('click', function () { sheet.classList.toggle('open'); });

    window.addEventListener('scroll', function () {
      var bar = document.querySelector('.top-app-bar');
      if (bar) bar.classList.toggle('scrolled', window.scrollY > 4);
    });
    window.addEventListener('resize', function () {
      var btn = document.querySelector('.tab.active');
      if (btn) moveIndicator(btn);
    });
  }

  /* ---------- 动态取色（Material You：壁纸种子 -> MD3 角色色） ---------- */
  var DYN_VARS = {
    '--md-primary': 'primary',
    '--md-on-primary': 'onPrimary',
    '--md-primary-container': 'primaryContainer',
    '--md-on-primary-container': 'onPrimaryContainer',
    '--md-secondary': 'secondary',
    '--md-on-secondary': 'onSecondary',
    '--md-secondary-container': 'secondaryContainer',
    '--md-on-secondary-container': 'onSecondaryContainer',
    '--md-tertiary': 'tertiary',
    '--md-on-tertiary': 'onTertiary',
    '--md-error': 'error',
    '--md-on-error': 'onError',
    '--md-error-container': 'errorContainer',
    '--md-on-error-container': 'onErrorContainer',
    '--md-surface': 'surface',
    '--md-on-surface': 'onSurface',
    '--md-surface-variant': 'surfaceVariant',
    '--md-on-surface-variant': 'onSurfaceVariant',
    '--md-surface-container-lowest': 'surfaceContainerLowest',
    '--md-surface-container-low': 'surfaceContainerLow',
    '--md-surface-container': 'surfaceContainer',
    '--md-surface-container-high': 'surfaceContainerHigh',
    '--md-surface-container-highest': 'surfaceContainerHighest',
    '--md-outline': 'outline',
    '--md-outline-variant': 'outlineVariant',
    '--md-inverse-surface': 'inverseSurface',
    '--md-inverse-on-surface': 'inverseOnSurface'
  };
  var DYN_VARIANTS = {
    tonal: function (h, d) { return new MC.SchemeTonalSpot(h, d, 0); },
    vibrant: function (h, d) { return new MC.SchemeVibrant(h, d, 0); },
    expressive: function (h, d) { return new MC.SchemeExpressive(h, d, 0); },
    content: function (h, d) { return new MC.SchemeContent(h, d, 0); },
    fidelity: function (h, d) { return new MC.SchemeFidelity(h, d, 0); },
    neutral: function (h, d) { return new MC.SchemeNeutral(h, d, 0); },
    rainbow: function (h, d) { return new MC.SchemeRainbow(h, d, 0); },
    monochrome: function (h, d) { return new MC.SchemeMonochrome(h, d, 0); }
  };
  var dyn = { on: false, variant: 'tonal', seed: 0, cache: null };

  function dynAvailable() { return !!(window.MC && window.MC.Hct && window.MC.SchemeTonalSpot); }

  function buildDyn(seed, variant) {
    if (!dynAvailable()) return null;
    var f = DYN_VARIANTS[variant] || DYN_VARIANTS.tonal;
    var s = seed >>> 0;
    return { dark: f(MC.Hct.fromInt(s), true), light: f(MC.Hct.fromInt(s), false) };
  }

  function schemeVars(s) {
    var o = {};
    if (!s || !dynAvailable()) return o;
    Object.keys(DYN_VARS).forEach(function (k) {
      var v = s[DYN_VARS[k]];
      if (typeof v === 'number') o[k] = MC.hexFromArgb(v);
    });
    var os = s.onSurface;
    o['--state'] = 'rgba(' + MC.redFromArgb(os) + ',' + MC.greenFromArgb(os) + ',' + MC.blueFromArgb(os) + ',0.12)';
    return o;
  }

  function applyVars(o) {
    var st = document.documentElement.style;
    Object.keys(o).forEach(function (k) { st.setProperty(k, o[k]); });
  }

  function applyDyn() {
    var root = document.documentElement;
    if (dyn.on && dyn.cache) {
      applyVars(schemeVars(root.getAttribute('data-theme') === 'light' ? dyn.cache.light : dyn.cache.dark));
    } else {
      Object.keys(DYN_VARS).forEach(function (k) { root.style.removeProperty(k); });
      root.style.removeProperty('--state');
    }
  }

  function updateSeedLabel() {
    var el = $('dyn-seed');
    if (!el) return;
    if (dyn.seed && dynAvailable()) el.textContent = '种子 ' + MC.hexFromArgb(dyn.seed);
    else el.textContent = dyn.on ? '未取色' : '已关闭';
  }

  function saveDyn() {
    var payload = { on: dyn.on, variant: dyn.variant, seed: dyn.seed };
    if (dyn.cache) { payload.dark = schemeVars(dyn.cache.dark); payload.light = schemeVars(dyn.cache.light); }
    try { localStorage.setItem('tca-dyn', JSON.stringify(payload)); } catch (e) {}
  }

  function fetchSeed() {
    progressOn();
    return ksuExec('cat /data/system/users/*/wallpaper_info.xml 2>/dev/null').then(function (r) {
      progressOff();
      var m = /colorValue0="(-?[0-9]+)"/.exec(r.stdout || '');
      if (!m) throw new Error('系统还没提取壁纸主色');
      return (parseInt(m[1], 10) >>> 0);
    }).catch(function (e) { progressOff(); throw e; });
  }

  function reloadDyn(seed) {
    dyn.seed = seed >>> 0;
    dyn.cache = buildDyn(dyn.seed, dyn.variant);
    applyDyn();
    saveDyn();
    updateSeedLabel();
  }

  function enableDyn(fromFetch) {
    if (!dynAvailable()) { toast('动态取色组件没加载', true); return Promise.resolve(); }
    if (dyn.seed && !fromFetch) {
      dyn.on = true;
      dyn.cache = buildDyn(dyn.seed, dyn.variant);
      applyDyn(); saveDyn(); updateSeedLabel();
      return Promise.resolve();
    }
    return fetchSeed().then(function (seed) {
      dyn.on = true;
      reloadDyn(seed);
      toast('动态取色：' + MC.hexFromArgb(seed));
    }).catch(function (e) {
      dyn.on = false;
      var cb = $('dyn-on'); if (cb) cb.checked = false;
      applyDyn(); saveDyn(); updateSeedLabel();
      toast('取色失败：' + e.message, true);
    });
  }

  function loadDyn() {
    if (!dynAvailable()) {
      var c0 = $('dyn-on'); if (c0) { c0.checked = false; c0.disabled = true; }
      updateSeedLabel();
      return Promise.resolve();
    }
    var raw = null;
    try { raw = localStorage.getItem('tca-dyn'); } catch (e) {}
    if (raw) {
      try {
        var o = JSON.parse(raw);
        dyn.on = !!o.on;
        dyn.variant = o.variant || 'tonal';
        dyn.seed = (o.seed || 0) >>> 0;
      } catch (e) {}
    } else {
      dyn.on = true;
      dyn.variant = 'tonal';
      dyn.seed = 0;
    }
    var cb = $('dyn-on'); if (cb) cb.checked = dyn.on;
    var vs = $('dyn-variant'); if (vs) vs.value = dyn.variant;
    updateSeedLabel();
    if (!dyn.on) { applyDyn(); saveDyn(); return Promise.resolve(); }
    if (dyn.seed) { dyn.cache = buildDyn(dyn.seed, dyn.variant); applyDyn(); saveDyn(); updateSeedLabel(); return Promise.resolve(); }
    return enableDyn(true);
  }

  function boot() {
    initTheme();
    bind();
    var active = document.querySelector('.tab.active');
    if (active) moveIndicator(active);
    if (!bridge) {
      var b = $('banner');
      if (b) {
        b.textContent = '未检测到 KernelSU/ReSukiSU 的 WebUI 桥接。请从 ReSukiSU/KernelSU 的模块页打开本页，或用支持 WebUI 的模块管理应用打开。';
        b.classList.remove('hidden');
      }
      log('[提示] 没有 WebUI 桥接，无法执行命令。');
      return;
    }
    resolveModDir().then(function (dir) {
      MODDIR = dir;
      log('[模块目录] ' + MODDIR);
      return ksuExec('uname -r');
    }).then(function (r) {
      var k = $('kinfo');
      if (k) k.textContent = (r.stdout || '').trim() || '内核版本未知';
      return loadDyn();
    }).then(function () {
      return readBoot();
    }).then(function () {
      return refresh();
    }).catch(function (e) {
      log('[初始化失败] ' + e.message);
      toast('初始化失败：' + e.message, true);
    });
  }

  boot();
})();
