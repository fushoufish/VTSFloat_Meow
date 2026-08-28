const menuButton = document.querySelector('.menu-toggle');
const sidebar = document.querySelector('.sidebar');
const navLinks = [...document.querySelectorAll('.nav-link')];
const sections = navLinks.map((link) => document.querySelector(link.getAttribute('href'))).filter(Boolean);
const languageSelect = document.querySelector('#language-select');
const rotatingText = document.querySelector('.hero-rotator-text');
const rotatingLines = [
  '跃入你的每一寸桌面日常',
  '赋予“Live2D”生命力',
  '展示你的专属看板娘',
  '让你的模型跃入日常触手可及',
  '重塑虚拟与现实的日常边界',
  '褪去“仅限开播”的束缚',
  '融入你的工作与游戏'
];
let rotatingIndex = Math.max(0, rotatingLines.indexOf(rotatingText?.textContent ?? ''));

const showNextLine = () => {
  if (!rotatingText) return;
  rotatingIndex = (rotatingIndex + 1) % rotatingLines.length;
  rotatingText.classList.remove('is-animating');
  void rotatingText.offsetWidth;
  rotatingText.textContent = rotatingLines[rotatingIndex];
  rotatingText.classList.add('is-animating');
};

if (rotatingText) {
  rotatingText.classList.add('is-animating');
  window.setInterval(showNextLine, 2600);
}

// Language switching is intentionally only reserved for now. The page keeps
// Chinese content until the translations are added to one shared config.
languageSelect?.addEventListener('change', () => {
  const languageName = languageSelect.options[languageSelect.selectedIndex].text;
  const toast = document.createElement('div');
  toast.className = 'language-toast';
  toast.textContent = `${languageName} 界面预留中当前页面暂时保持中文`;
  document.body.appendChild(toast);
  window.setTimeout(() => toast.remove(), 2600);
});

menuButton?.addEventListener('click', () => {
  const isOpen = sidebar.classList.toggle('open');
  menuButton.setAttribute('aria-expanded', String(isOpen));
});

navLinks.forEach((link) => link.addEventListener('click', () => {
  sidebar.classList.remove('open');
  menuButton?.setAttribute('aria-expanded', 'false');
}));

const updateActiveLink = () => {
  const marker = window.scrollY + 150;
  let current = sections[0]?.id;
  sections.forEach((section) => { if (section.offsetTop <= marker) current = section.id; });
  navLinks.forEach((link) => link.classList.toggle('active', link.getAttribute('href') === `#${current}`));
};

window.addEventListener('scroll', updateActiveLink, { passive: true });
updateActiveLink();
