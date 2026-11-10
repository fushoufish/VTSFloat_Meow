(function () {
  'use strict';

  const loader = document.getElementById('resource-loader');
  if (!loader) {
    return;
  }

  const progressLabels = {
    'zh-CN': '页面资源加载进度',
    'zh-TW': '頁面資源載入進度',
    en: 'Page resource loading progress',
    ja: 'ページ素材の読み込み状況',
    ko: '페이지 리소스 불러오기 진행률',
    ru: 'Загрузка ресурсов страницы'
  };

  const supported = Object.keys(progressLabels);
  const normalizeLocale = (value) => {
    const locale = String(value || '').toLowerCase();
    if (locale.startsWith('zh-hant') || /^(zh-)?(tw|hk|mo)/.test(locale)) return 'zh-TW';
    if (locale.startsWith('zh')) return 'zh-CN';
    if (locale.startsWith('ja')) return 'ja';
    if (locale.startsWith('ko')) return 'ko';
    if (locale.startsWith('ru')) return 'ru';
    return 'en';
  };

  let storedLocale = '';
  try {
    storedLocale = localStorage.getItem('vtsfloat-docs-language') || '';
  } catch (_) {
    storedLocale = '';
  }
  const browserLocale = (navigator.languages && navigator.languages[0]) || navigator.language || 'en';
  const locale = supported.includes(storedLocale) ? storedLocale : normalizeLocale(browserLocale);
  const progressLabel = progressLabels[locale];
  const progress = loader.querySelector('[data-loader-progress]');
  const progressbar = loader.querySelector('[role="progressbar"]');
  const progressText = loader.querySelector('[data-loader-percent]');
  loader.setAttribute('aria-label', progressLabel);
  progressbar.setAttribute('aria-label', progressLabel);
  loader.dataset.stage = 'downloading';
  loader.setAttribute('aria-busy', 'true');

  const toAbsoluteUrl = (value) => {
    if (!value || value.startsWith('data:') || value.startsWith('blob:')) return '';
    try {
      return new URL(value, document.baseURI).href;
    } catch (_) {
      return '';
    }
  };

  const urls = new Set();
  document.querySelectorAll('img[src], video[src], source[src]').forEach((element) => {
    const url = toAbsoluteUrl(element.getAttribute('src'));
    if (url) urls.add(url);
  });
  document.querySelectorAll('img[srcset], source[srcset]').forEach((element) => {
    element.getAttribute('srcset').split(',').forEach((candidate) => {
      const url = toAbsoluteUrl(candidate.trim().split(/\s+/)[0]);
      if (url) urls.add(url);
    });
  });
  document.querySelectorAll('video[poster]').forEach((element) => {
    const url = toAbsoluteUrl(element.getAttribute('poster'));
    if (url) urls.add(url);
  });
  document.querySelectorAll('link[rel~="stylesheet"][href], link[rel~="icon"][href], link[rel="apple-touch-icon"][href]').forEach((element) => {
    const url = toAbsoluteUrl(element.getAttribute('href'));
    if (url) urls.add(url);
  });

  document.querySelectorAll('script[src]').forEach((script) => {
    const url = toAbsoluteUrl(script.getAttribute('src'));
    if (url) urls.add(url);
  });

  const assets = Array.from(urls).filter(Boolean);
  let completed = 0;
  let failures = 0;

  const updateProgress = () => {
    const value = assets.length ? Math.round((completed / assets.length) * 100) : 100;
    progress.style.width = `${value}%`;
    progressbar.setAttribute('aria-valuenow', String(value));
    if (progressText) progressText.textContent = `${value}%`;
  };

  const fetchAsset = async (url) => {
    const controller = new AbortController();
    const timeout = window.setTimeout(() => controller.abort(), 120000);
    try {
      const response = await fetch(url, {
        cache: 'force-cache',
        credentials: new URL(url).origin === window.location.origin ? 'same-origin' : 'omit',
        mode: new URL(url).origin === window.location.origin ? 'same-origin' : 'no-cors',
        signal: controller.signal
      });
      if (!response.ok && response.type !== 'opaque') throw new Error(`HTTP ${response.status}`);
      if (response.type !== 'opaque') await response.arrayBuffer();
    } catch (_) {
      failures += 1;
    } finally {
      window.clearTimeout(timeout);
      completed += 1;
      updateProgress();
    }
  };

  const runQueue = async () => {
    let next = 0;
    const worker = async () => {
      while (next < assets.length) {
        const index = next;
        next += 1;
        await fetchAsset(assets[index]);
      }
    };
    await Promise.all(Array.from({ length: Math.min(4, Math.max(assets.length, 1)) }, worker));
  };

  const waitForPage = document.readyState === 'complete'
    ? Promise.resolve()
    : new Promise((resolve) => window.addEventListener('load', resolve, { once: true }));
  const minimumDisplay = new Promise((resolve) => window.setTimeout(resolve, 600));

  updateProgress();
  Promise.all([runQueue(), waitForPage, minimumDisplay])
    .catch(() => {
      failures += 1;
    })
    .then(() => {
      completed = assets.length;
      updateProgress();
      loader.setAttribute('aria-busy', 'false');
      window.dispatchEvent(new CustomEvent('vtsfloat:assetsready', {
        detail: { total: assets.length, failures }
      }));
      window.setTimeout(() => {
        loader.classList.add('is-leaving');
        window.setTimeout(() => loader.remove(), 480);
      }, 280);
    });
})();
