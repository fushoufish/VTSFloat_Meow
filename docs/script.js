const menuButton = document.querySelector('.menu-toggle');
const sidebar = document.querySelector('.sidebar');
const navLinks = [...document.querySelectorAll('.nav-link')];
const sections = navLinks.map((link) => document.querySelector(link.getAttribute('href'))).filter(Boolean);
const languageSelect = document.querySelector('#language-select');
const rotatingText = document.querySelector('.hero-rotator-text');
let rotatingLines = window.VTSFloatI18n?.getHeroLines() || [rotatingText?.textContent || 'VTSFloat_Meow'];
let rotatingIndex = 0;
let rotatingTimer = null;

const showNextLine = () => {
  if (!rotatingText) return;
  rotatingIndex = (rotatingIndex + 1) % rotatingLines.length;
  rotatingText.classList.remove('is-animating');
  void rotatingText.offsetWidth;
  rotatingText.textContent = rotatingLines[rotatingIndex];
  rotatingText.classList.add('is-animating');
};

const restartHeroRotation = () => {
  window.clearInterval(rotatingTimer);
  rotatingTimer = null;
  rotatingIndex = 0;
  if (!rotatingText) return;
  rotatingText.textContent = rotatingLines[0];
  rotatingText.classList.toggle('is-animating', rotatingLines.length > 1);
  if (rotatingLines.length > 1) rotatingTimer = window.setInterval(showNextLine, 2600);
};

restartHeroRotation();

menuButton?.addEventListener('click', () => {
  const isOpen = sidebar.classList.toggle('open');
  menuButton.setAttribute('aria-expanded', String(isOpen));
});

navLinks.forEach((link) => link.addEventListener('click', () => {
  sidebar.classList.remove('open');
  menuButton?.setAttribute('aria-expanded', 'false');
}));

const previewRoot = document.querySelector('.main-content');
const previewImages = previewRoot
  ? [...previewRoot.querySelectorAll('.capture-media img')]
  : [];

previewImages.forEach((image) => {
  image.classList.add('previewable-image');
  const link = image.closest('a');
  if (!link) return;

  image.dataset.previewUrl = link.getAttribute('href') || image.currentSrc || image.src;
  link.removeAttribute('href');
  link.removeAttribute('target');
  link.removeAttribute('rel');
  link.setAttribute('role', 'button');
  link.setAttribute('tabindex', '0');
  link.setAttribute('aria-label', `${image.alt || ''} — ${window.VTSFloatI18n?.ui('imagePreview') || ''}`);
  link.addEventListener('keydown', (event) => {
    if (event.key !== 'Enter' && event.key !== ' ') return;
    event.preventDefault();
    image.click();
  });
});

if (previewRoot && previewImages.length && window.Viewer) {
  new Viewer(previewRoot, {
    filter: (image) => image.classList.contains('previewable-image'),
    url: (image) => image.dataset.previewUrl || image.currentSrc || image.src,
    backdrop: true,
    button: true,
    focus: true,
    fullscreen: false,
    keyboard: true,
    loop: true,
    movable: true,
    navbar: false,
    rotatable: false,
    scalable: false,
    slideOnTouch: true,
    title: [1, (image) => image.alt || ''],
    toggleOnDblclick: true,
    toolbar: {
      zoomIn: 1,
      zoomOut: 1,
      oneToOne: 1,
      reset: 1,
      prev: 1,
      next: 1,
    },
    tooltip: true,
    transition: true,
    zoomable: true,
    zoomOnTouch: true,
    zoomOnWheel: true,
    zoomRatio: 0.08,
  });
}

const videos = [...document.querySelectorAll('video')];
videos.forEach((video) => {
  video.disablePictureInPicture = true;
  video.disableRemotePlayback = true;
  video.setAttribute('disablepictureinpicture', '');
  video.setAttribute('disableremoteplayback', '');
  video.setAttribute('controlslist', 'nodownload nofullscreen noremoteplayback');
  video.setAttribute('x-webkit-airplay', 'deny');
  video.controls = false;
  video.setAttribute('role', 'button');
  video.setAttribute('tabindex', '0');
  video.setAttribute('aria-label', `${video.getAttribute('aria-label') || ''}${window.VTSFloatI18n?.ui('videoHint') || ''}`);

  const revealControls = () => {
    if (video.controls) return;
    video.controls = true;
    video.removeAttribute('role');
    video.removeAttribute('tabindex');
    const hint = window.VTSFloatI18n?.ui('videoHint') || '';
    video.setAttribute('aria-label', video.getAttribute('aria-label').replace(hint, ''));
  };

  video.addEventListener('click', revealControls, { once: true });
  video.addEventListener('keydown', (event) => {
    if (event.key !== 'Enter' && event.key !== ' ') return;
    event.preventDefault();
    revealControls();
  });
});

window.addEventListener('vtsfloat:languagechange', () => {
  rotatingLines = window.VTSFloatI18n?.getHeroLines() || rotatingLines;
  restartHeroRotation();
  previewImages.forEach((image) => {
    const link = image.closest('a[role="button"]');
    if (link) link.setAttribute('aria-label', `${image.alt || ''} — ${window.VTSFloatI18n?.ui('imagePreview') || ''}`);
  });
  videos.forEach((video) => {
    if (!video.controls) video.setAttribute('aria-label', `${video.getAttribute('aria-label') || ''}${window.VTSFloatI18n?.ui('videoHint') || ''}`);
  });
});

const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');

const revealSelectors = [
  '.main-content > section:not(.hero) > .section-heading',
  '.usage-custom-grid > .usage-substep',
  '.usage-group > .usage-group-heading',
  '.usage-group > .capture-media',
  '.usage-substeps > .usage-substep',
  '.other-notes-copy > *',
  '.gpu-explanation > *',
  '.readme-section > .callout',
  '.pipeline-system-map',
  '.tech-table > div',
  '.benchmark-carousel',
  '.benchmark-conclusion > p',
  '.benchmark-video-section',
  '.requirements-grid > .info-card',
  '.footer',
];
const revealItems = [...new Set(document.querySelectorAll(revealSelectors.join(',')))];

if (!reducedMotion.matches && 'IntersectionObserver' in window) {
  const groupIndexes = new Map();
  revealItems.forEach((item) => {
    const group = item.parentElement;
    const index = groupIndexes.get(group) || 0;
    groupIndexes.set(group, index + 1);
    item.classList.add('scroll-reveal');
    item.style.setProperty('--reveal-delay', `${Math.min(index, 4) * 70}ms`);
  });

  const revealObserver = new IntersectionObserver((entries, observer) => {
    entries.forEach((entry) => {
      if (!entry.isIntersecting) return;
      const item = entry.target;
      item.classList.add('is-revealed');
      item.addEventListener('animationend', () => {
        item.classList.remove('scroll-reveal', 'is-revealed');
        item.style.removeProperty('--reveal-delay');
      }, { once: true });
      observer.unobserve(item);
    });
  }, {
    rootMargin: '0px 0px -9% 0px',
    threshold: 0.08,
  });

  revealItems.forEach((item) => revealObserver.observe(item));
}

const updateActiveLink = () => {
  const pageBottom = document.documentElement.scrollHeight - (window.scrollY + window.innerHeight);
  let current;
  if (pageBottom <= 4) {
    current = sections.at(-1)?.id;
  } else {
    const marker = window.scrollY + 150;
    current = sections[0]?.id;
    sections.forEach((section) => { if (section.offsetTop <= marker) current = section.id; });
  }
  navLinks.forEach((link) => link.classList.toggle('active', link.getAttribute('href') === `#${current}`));
};

window.addEventListener('scroll', updateActiveLink, { passive: true });
window.addEventListener('resize', updateActiveLink);
updateActiveLink();
