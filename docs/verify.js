(() => {
  const repository = 'fushoufish/VTSFloat_Meow';
  const api = 'https://api.github.com/repos/' + repository;
  const releases = 'https://github.com/' + repository + '/releases';
  const exeName = 'VTSFloat_Meow.exe';
  const drop = document.getElementById('verify-drop');
  if (!drop) return;
  const input = document.getElementById('verify-file');
  const prompt = document.getElementById('verify-prompt');
  const result = document.getElementById('verify-result');
  const reveal = result.closest('.verify-reveal');
  const faq = reveal.querySelector('.verify-faq');
  const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
  const status = document.getElementById('verify-status');
  const detail = document.getElementById('verify-detail');
  const facts = document.getElementById('verify-facts');
  const name = document.getElementById('verify-name');
  const hash = document.getElementById('verify-hash');
  const source = document.getElementById('verify-source');
  const release = document.getElementById('verify-release');
  const proof = document.getElementById('verify-proof');
  const updateNotice = document.getElementById('verify-update');

  const translate = (key) => window.VTSFloatI18n?.translate(key) ?? key;
  let selection = 0;
  let pending;
  let dragDepth = 0;
  let view;
  let revealAnimation;

  function render() {
    if (!view) return;
    const { state, title, description, file, digest, tag, provenance, updateAvailable } = view;
    const previousHeight = reveal.getBoundingClientRect().height;
    revealAnimation?.cancel();
    prompt.textContent = translate(state === 'loading' ? title : '点击选择文件');
    result.hidden = state === 'loading';
    result.dataset.state = state;
    status.textContent = translate(title);
    detail.textContent = translate(description).replace('{version}', () => tag ?? '');
    facts.hidden = !digest;
    faq.hidden = !digest;
    name.textContent = file?.name ?? '';
    hash.textContent = digest ?? '';
    source.textContent = provenance ? translate(provenance) : '—';
    release.hidden = !tag;
    updateNotice.hidden = !updateAvailable;
    proof.hidden = provenance !== 'GitHub 已记录构建证明';
    if (tag) release.href = releases + '/tag/' + encodeURIComponent(tag);

    if (reducedMotion.matches || result.hidden) return;
    const nextHeight = reveal.getBoundingClientRect().height;
    if (Math.abs(nextHeight - previousHeight) < 1) return;
    revealAnimation = reveal.animate([
      { height: previousHeight + 'px', opacity: previousHeight === 0 ? 0 : 1 },
      { height: nextHeight + 'px', opacity: 1 }
    ], {
      duration: previousHeight === 0 ? 340 : 220,
      easing: 'cubic-bezier(.2, .75, .25, 1)'
    });
  }

  async function getJson(url, signal) {
    const controller = new AbortController();
    const abort = () => controller.abort();
    const timeout = setTimeout(abort, 6500);
    if (signal.aborted) abort();
    else signal.addEventListener('abort', abort, { once: true });
    try {
      const response = await fetch(url, {
        signal: controller.signal,
        credentials: 'omit',
        referrerPolicy: 'no-referrer'
      });
      if (!response.ok) throw new Error('HTTP ' + response.status);
      return await response.json();
    } finally {
      clearTimeout(timeout);
      signal.removeEventListener('abort', abort);
    }
  }

  async function lookupRelease(file, digest, signal) {
    let complete = true;
    for (let page = 1; ; page++) {
      const list = await getJson(api + '/releases?per_page=100&page=' + page, signal);
      if (!Array.isArray(list)) throw new Error('Invalid release list');
      for (const entry of list) {
        if (entry.draft) continue;
        if (typeof entry.tag_name !== 'string' || !Array.isArray(entry.assets)) {
          complete = false;
          continue;
        }
        for (const asset of entry.assets) {
          if (asset.name !== exeName || asset.state !== 'uploaded') continue;
          if (typeof asset.digest !== 'string' || !/^sha256:[a-f0-9]{64}$/i.test(asset.digest)) {
            complete = false;
            continue;
          }
          if (asset.size === file.size && asset.digest.toLowerCase() === 'sha256:' + digest) {
            return { tag: entry.tag_name, complete: true };
          }
        }
      }
      if (list.length < 100) return { complete };
    }
  }

  async function checkProvenance(digest, signal) {
    try {
      const entry = await getJson(api + '/attestations/sha256:' + digest + '?predicate_type=provenance', signal);
      if (!Array.isArray(entry.attestations)) return '暂时无法查询构建证明';
      return entry.attestations.length ? 'GitHub 已记录构建证明' : '未找到构建证明';
    } catch (error) {
      return error.message === 'HTTP 404' ? '未找到构建证明' : '暂时无法查询构建证明';
    }
  }

  function compareVersions(left, right) {
    const pattern = /^(v|beta)?(\d+)\.(\d+)(?:\.(\d+))?$/i;
    const a = pattern.exec(left);
    const b = pattern.exec(right);
    if (!a || !b) return 0;
    for (let i = 2; i <= 4; i++) {
      const difference = Number(a[i] ?? 0) - Number(b[i] ?? 0);
      if (difference) return difference;
    }
    return Number(b[1]?.toLowerCase() === 'beta') - Number(a[1]?.toLowerCase() === 'beta');
  }

  async function hasReleaseUpdate(currentTag, signal) {
    try {
      const latest = await getJson(api + '/releases/latest', signal);
      return typeof latest.tag_name === 'string' &&
        compareVersions(latest.tag_name, currentTag) > 0 &&
        Array.isArray(latest.assets) &&
        latest.assets.some((asset) => asset.name === exeName && asset.state === 'uploaded');
    } catch {
      return false;
    }
  }

  async function verify(files) {
    const id = ++selection;
    pending?.abort();
    pending = new AbortController();
    const { signal } = pending;
    const show = (next) => {
      if (id !== selection) return;
      view = next;
      render();
    };
    if (files.length !== 1 || !/\.exe$/i.test(files[0].name)) {
      show({ state: 'error', title: '请选择一个 EXE 文件', description: '每次可以校验一个 Windows EXE 文件。' });
      return;
    }
    const file = files[0];
    if (file.size > 128 * 1024 * 1024) {
      show({ state: 'error', title: '文件过大', description: '请选择不超过 128 MB 的 EXE 文件。' });
      return;
    }
    show({ state: 'loading', title: '正在校验文件…', description: '正在本地计算 SHA-256 并比对官方发布。' });
    let digest;
    try {
      const bytes = await crypto.subtle.digest('SHA-256', await file.arrayBuffer());
      digest = Array.from(new Uint8Array(bytes), (byte) => byte.toString(16).padStart(2, '0')).join('');
    } catch {
      show({ state: 'error', title: '无法读取文件', description: '请在 HTTPS 页面中重新选择文件。' });
      return;
    }
    if (signal.aborted) return;

    let lookup;
    try {
      lookup = await lookupRelease(file, digest, signal);
    } catch {
      if (signal.aborted) return;
      lookup = { complete: false };
      try {
        const snapshot = await getJson('release-checksums.json', signal);
        const match = snapshot.find((entry) => entry.size === file.size && entry.sha256 === digest);
        if (match) lookup = { tag: match.tag };
      } catch {}
    }

    if (lookup.tag) {
      const matched = {
        state: 'match',
        title: '与VTSFloat_Meow的Github发布的开源版本一致',
        description: '结果：校验无误，版本为{version}',
        file, digest, tag: lookup.tag, provenance: '正在查询构建证明…'
      };
      show(matched);
      if (signal.aborted) return;
      await Promise.all([
        checkProvenance(digest, signal).then((provenance) => {
          matched.provenance = provenance;
          show(matched);
        }),
        hasReleaseUpdate(matched.tag, signal).then((updateAvailable) => {
          matched.updateAvailable = updateAvailable;
          show(matched);
        })
      ]);
    } else {
      show({
        state: lookup.complete ? 'mismatch' : 'error',
        title: lookup.complete ? '未匹配到官方发布' : '暂时无法确认',
        description: lookup.complete
          ? '没有找到相同的官方 EXE。自编译版或修改版也可能显示此结果。'
          : '官方发布列表暂时无法完整查询，请稍后重试。',
        file, digest
      });
    }
  }

  updateNotice.addEventListener('click', () => {
    window.scrollTo({ top: 0, behavior: 'instant' });
    document.getElementById('download-button').click();
  });

  drop.addEventListener('click', () => input.click());
  input.addEventListener('change', () => {
    if (input.files.length) void verify(input.files);
    input.value = '';
  });
  drop.addEventListener('dragenter', (event) => {
    if (!event.dataTransfer?.types.includes('Files')) return;
    event.preventDefault();
    dragDepth++;
    drop.dataset.dragging = '';
  });
  drop.addEventListener('dragleave', () => {
    dragDepth = Math.max(0, dragDepth - 1);
    if (!dragDepth) delete drop.dataset.dragging;
  });
  drop.addEventListener('dragover', (event) => {
    if (event.dataTransfer?.types.includes('Files')) event.preventDefault();
  });
  drop.addEventListener('drop', (event) => {
    if (!event.dataTransfer?.files.length) return;
    event.preventDefault();
    dragDepth = 0;
    delete drop.dataset.dragging;
    void verify(event.dataTransfer.files);
  });
  window.addEventListener('vtsfloat:languagechange', render);
})();
