(() => {
  'use strict';

  const locale = window.VTSFloatI18n?.getLocale();
  const useCnb = ['zh-CN', 'zh-TW'].includes(locale) &&
    ['fushoufish.github.io', 'vtsfloatmeow.edgeone.dev'].includes(location.hostname);
  const mirror = 'https://cdn.jsdmirror.cn/cnb/fushoufish/VTSFloat_Meow@main/docs/';

  document.querySelectorAll('[data-media-src]').forEach((media) => {
    const original = new URL(media.dataset.mediaSrc, document.baseURI);
    if (useCnb) {
      media.crossOrigin = 'anonymous';
      media.addEventListener('error', () => {
        media.removeAttribute('crossorigin');
        media.src = original.href;
      }, { once: true });
    }
    if (media instanceof HTMLVideoElement && media.autoplay) {
      media.preload = 'auto';
    }
    media.src = useCnb ? new URL(media.dataset.mediaSrc, mirror).href : original.href;
  });
})();
