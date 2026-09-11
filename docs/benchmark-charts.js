(() => {
  const root = document.querySelector('[data-benchmark-carousel]');
  if (!root) return;

  const data = {
    source: ['3840x2160', '2560x1440', '1920x1080', '1600x900', '1280x720', '960x540', '640x480'],
    render: ['640x480', '960x540', '1280x720', '1600x900', '1920x1080', '2560x1440', '3840x2160'],
    totalLatency: [[3.044,3.287,3.718,4.727,5.478,8.551,20.33],[1.134,1.466,2.207,3.202,4.289,7.447,18.814],[0.589,0.863,1.627,2.467,3.864,7.185,18.269],[0.483,0.724,1.457,2.269,3.507,6.995,18.217],[0.458,0.657,1.364,2.113,3.418,6.907,18.004],[0.459,0.629,1.4,2.33,3.612,6.887,17.751],[0.434,0.618,1.563,2.328,3.656,6.768,17.657]],
    fps: [[179.588,184.103,170.16,142.835,128.753,90.946,42.077],[240.176,240.188,226.399,180.377,150.389,99.813,44.534],[240.178,240.171,240.148,207.255,162.368,101.493,45.946],[240.177,240.18,240.169,213.543,168.995,103.204,45.927],[240.171,240.171,240.183,219.544,170.451,103.535,46.464],[240.137,240.195,226.793,176.563,153.7,101.281,47.178],[240.154,240.147,201.392,174.067,154.273,103.684,47.576]],
    gpu: [[47.351,54.382,52.697,54.437,58.247,59.734,52.741],[31.449,37.8,47.351,48.8,53,55.857,51.109],[24.9,31.1,43.6,49.1,52.2,53.7,52],[22.9,29.1,41.7,48.8,52.7,53.9,51.7],[21.9,28.2,40.7,48.4,51.9,53.2,52],[20.9,27.4,38.5,39.9,46.7,52.3,52.5],[20.1,26.8,34.3,39.4,46.2,53.1,53.2]],
    receive: [0.108,0.11,0.108,0.115,0.113,0.122,0.143],
    scale: [0.753,0.964,1.436,2.093,2.992,5.258,12.175],
    present: [0.082,0.104,0.362,0.569,0.87,1.869,6.116],
    programFps: [231.512,232.165,220.749,187.741,155.561,100.565,45.672]
  };

  const alignmentData = {
    labels: [...data.source, ...data.render.slice(1)],
    alignment: [3.704,8.333,14.815,21.333,33.333,59.259,100,59.259,33.333,21.333,14.815,8.333,3.704],
    fps: [...data.fps.map((row) => row[0]), ...data.fps[data.fps.length - 1].slice(1)],
    latency: [...data.totalLatency.map((row) => row[0]), ...data.totalLatency[data.totalLatency.length - 1].slice(1)]
  };

  const translations = {
    'zh-CN': { datasetTitle:'AMD Radeon Graphics 分辨率基准', method:'测试平台 AMD Ryzen 7 9800X3D / DDR5-6400 CL28', caveat:'影响性能的因素包括 VTS 模型面数、渲染特效、挂件数量、物理效果及系统中的其他应用。采集过程中已尽量避免不平衡因素，包括暂停动捕。', conclusion:'VTS原生分辨率：VTSM固定480p时，VTS由4K降至480p，帧率约180→240，延迟3.04→0.43ms。接收始终很快，但高分辨率原生纹理需读取更多像素并承担更重的缩小滤波，所以即使输出尺寸不变，高VTS画布仍增加GPU负担。这说明VTS源尺寸仍是次要影响因素。若模型没有明显画质收益，可降低VTS输出换取稳定帧率。\nVTSM分辨率：VTS固定480p时，VTSM由480p升至4K，帧率约240→48，延迟0.62→17.66ms。接收耗时变化很小，增长主要来自GPU缩放和Windows透明窗口上屏；1440p后负担明显增加，4K时两者成为主要瓶颈。GPU占用升高的同时，桌面合成调度也会进一步限制帧率。因此VTSM处理和提交的像素量，比VTS输入尺寸更直接决定性能。\n总结：两端越接近，越能减少无效缩放，使帧率和延迟更稳定；但对齐率不是唯一标准。相同对齐率下，低输出仍更快，4K即使完全对齐也需处理大量像素。日常推荐1280×720，兼顾清晰度、低延迟和高帧率；细节优先可选1920×1080。否则不建议长期使用高分辨率输出与高分辨率缩放，并应让VTS与VTSM的分辨率尺寸尽量接近。', latency:'总处理延迟（ms）', latencyAxis:'延迟（ms）', fps:'VTSFloat_Meow 实际帧率（FPS）', components:'输出分辨率增加时的延迟构成', gpu:'GPU 占用率热力图（%）', source:'VTS 原生分辨率', render:'VTSFloat_Meow 渲染分辨率', receive:'接收', scale:'缩放', present:'上屏', programFps:'程序帧率（FPS）', previous:'上一张图表', next:'下一张图表', choose:'选择图表', show:'显示第 {n} 张图表' },
    'zh-TW': { datasetTitle:'AMD Radeon Graphics 解析度效能測試', method:'測試平台：AMD Ryzen 7 9800X3D / DDR5-6400 CL28', caveat:'效能也會受到 VTS 模型面數、渲染特效、掛件數量、物理效果及其他執行中應用程式影響。採集時已盡量排除不均衡因素，包括暫停動作捕捉。', conclusion:'VTS 原生解析度：VTSM 固定為 480p 時，將 VTS 從 4K 降至 480p，幀率約由 180 提升至 240 FPS，延遲由 3.04 降至 0.43 ms。接收階段始終很快，但高解析度原生紋理需要讀取更多像素，也要承擔更繁重的縮小濾波；因此，即使輸出尺寸不變，較大的 VTS 畫布仍會增加 GPU 負擔。這表示 VTS 來源尺寸仍是次要影響因素。若模型沒有明顯的畫質收益，可降低 VTS 輸出以換取更穩定的幀率。\nVTSM 解析度：VTS 固定為 480p 時，將 VTSM 從 480p 提升至 4K，幀率約由 240 降至 48 FPS，延遲由 0.62 增至 17.66 ms。接收耗時變化很小，增加的負擔主要來自 GPU 縮放與 Windows 透明視窗顯示；1440p 之後負擔明顯上升，到 4K 時兩者成為主要瓶頸。GPU 使用率提高的同時，桌面合成排程也會進一步限制幀率。因此，VTSM 處理與提交的像素量，比 VTS 輸入尺寸更直接地決定效能。\n總結：兩端解析度越接近，越能減少不必要的縮放，讓幀率與延遲更穩定；但對齊率並非唯一標準。對齊率相同時，較低的輸出仍然更快；即使完全對齊，4K 仍需處理大量像素。日常建議使用 1280×720，在清晰度、低延遲與高幀率之間取得平衡；重視細節時可選擇 1920×1080。除非確有需要，不建議長時間使用高解析度輸出與高解析度縮放，並應讓 VTS 與 VTSM 的解析度尺寸盡量接近。', latency:'總處理延遲（毫秒）', latencyAxis:'延遲（毫秒）', fps:'VTSFloat_Meow 實際幀率（FPS）', components:'輸出解析度提升時的延遲構成', gpu:'GPU 使用率熱圖（%）', source:'VTS 原生解析度', render:'VTSFloat_Meow 渲染解析度', receive:'接收', scale:'縮放', present:'顯示', programFps:'程式幀率（FPS）', previous:'上一張圖表', next:'下一張圖表', choose:'選擇圖表', show:'顯示第 {n} 張圖表' },
    en: { datasetTitle:'AMD Radeon Graphics Resolution Benchmark', method:'Test platform: AMD Ryzen 7 9800X3D / DDR5-6400 CL28', caveat:'Performance also depends on VTS model polygon count, render effects, accessory count, physics, and other running applications. The capture minimized uneven factors where possible, including pausing motion capture.', conclusion:'Native VTS resolution: With VTSM fixed at 480p, reducing VTS from 4K to 480p raises the frame rate from about 180 to 240 FPS and cuts latency from 3.04 to 0.43 ms. Reception remains fast, but a larger native texture requires more pixel reads and heavier downsampling. A high-resolution VTS canvas therefore adds GPU work even when the output size does not change. Source resolution is still a secondary performance factor; if the model gains little visible detail, lowering the VTS output can deliver steadier frame rates.\nVTSM render resolution: With VTS fixed at 480p, increasing VTSM from 480p to 4K drops the frame rate from about 240 to 48 FPS and raises latency from 0.62 to 17.66 ms. Reception time barely changes. Most of the increase comes from GPU scaling and presenting the transparent Windows layer. The cost becomes pronounced beyond 1440p, and both stages are major bottlenecks at 4K. Higher GPU utilization also leaves less scheduling time for desktop composition. As a result, the number of pixels processed and presented by VTSM affects performance more directly than the VTS input size.\nSummary: Keeping the two resolutions close avoids unnecessary scaling and generally stabilizes frame rate and latency, but alignment is not the only consideration. At the same alignment ratio, a lower output still runs faster, and even perfectly matched 4K must process many pixels. 1280×720 is the everyday recommendation for a useful balance of clarity, low latency, and high frame rate; choose 1920×1080 when detail matters more. Avoid sustained high-resolution rendering and scaling unless needed, and keep the VTS and VTSM dimensions as close as practical.', latency:'Total processing latency (ms)', latencyAxis:'Latency (ms)', fps:'VTSFloat_Meow actual frame rate (FPS)', components:'Latency components by output resolution', gpu:'GPU utilization heatmap (%)', source:'VTS native resolution', render:'VTSFloat_Meow render resolution', receive:'Receive', scale:'Scale', present:'Present', programFps:'Application FPS', previous:'Previous chart', next:'Next chart', choose:'Choose chart', show:'Show chart {n}' },
    ja: { datasetTitle:'AMD Radeon Graphics 解像度ベンチマーク', method:'テスト環境：AMD Ryzen 7 9800X3D / DDR5-6400 CL28', caveat:'性能は VTS モデルのポリゴン数、描画効果、アクセサリー数、物理演算、ほかの実行中アプリにも左右されます。計測ではモーショントラッキングの停止など、偏りにつながる要因を可能な限り抑えました。', conclusion:'VTSのネイティブ解像度：VTSMを480pに固定した状態でVTSを4Kから480pへ下げると、フレームレートは約180から240 FPSへ上がり、遅延は3.04から0.43 msへ短縮します。受信処理は一貫して高速ですが、高解像度のネイティブテクスチャは読み込む画素数が多く、縮小フィルタリングの負荷も増えます。そのため出力サイズが同じでも、大きなVTSキャンバスはGPU負荷を高めます。VTSの入力サイズは二次的な要因ですが、見た目の向上が少ない場合はVTSの出力を下げることでフレームレートを安定させられます。\nVTSMの描画解像度：VTSを480pに固定した状態でVTSMを480pから4Kへ上げると、フレームレートは約240から48 FPSへ下がり、遅延は0.62から17.66 msへ増加します。受信時間はほとんど変わらず、増加分の中心はGPUによる拡大縮小とWindows透過ウィンドウの表示です。1440pを超えると負荷が目立ち、4Kではこの2つが主なボトルネックになります。GPU使用率の上昇によりデスクトップ合成のスケジューリングも厳しくなるため、VTSの入力サイズより、VTSMが処理して表示する画素数のほうが性能を直接左右します。\nまとめ：両者の解像度を近づけると不要な拡大縮小が減り、フレームレートと遅延が安定しやすくなります。ただし一致率だけで性能は決まりません。同じ一致率でも低い出力解像度のほうが速く、完全に一致した4Kでも大量の画素処理が必要です。普段使いには、鮮明さ・低遅延・高フレームレートのバランスがよい1280×720を推奨します。細部を優先する場合は1920×1080が適しています。必要がなければ高解像度での出力と拡大縮小を常用せず、VTSとVTSMの解像度をできるだけ近づけてください。', latency:'総処理遅延（ms）', latencyAxis:'遅延（ms）', fps:'VTSFloat_Meow 実測フレームレート（FPS）', components:'出力解像度別の遅延構成', gpu:'GPU 使用率ヒートマップ（%）', source:'VTS ネイティブ解像度', render:'VTSFloat_Meow 描画解像度', receive:'受信', scale:'拡大縮小', present:'表示', programFps:'アプリ FPS', previous:'前のグラフ', next:'次のグラフ', choose:'グラフを選択', show:'グラフ {n} を表示' },
    ko: { datasetTitle:'AMD Radeon Graphics 해상도 벤치마크', method:'테스트 환경: AMD Ryzen 7 9800X3D / DDR5-6400 CL28', caveat:'성능은 VTS 모델의 폴리곤 수, 렌더링 효과, 액세서리 수, 물리 효과, 실행 중인 다른 앱에 따라 달라집니다. 측정 중에는 모션 캡처를 일시 중지하는 등 불균형 요인을 최대한 줄였습니다.', conclusion:'VTS 원본 해상도: VTSM을 480p로 고정한 상태에서 VTS를 4K에서 480p로 낮추면 프레임률은 약 180에서 240 FPS로 오르고 지연 시간은 3.04에서 0.43 ms로 줄어듭니다. 수신 처리는 계속 빠르지만, 고해상도 원본 텍스처는 더 많은 픽셀을 읽고 더 무거운 축소 필터링을 수행해야 합니다. 따라서 출력 크기가 같아도 큰 VTS 캔버스는 GPU 부담을 높입니다. VTS 입력 크기는 부차적인 성능 요인이므로, 모델의 화질 향상이 뚜렷하지 않다면 VTS 출력을 낮춰 프레임률을 안정시킬 수 있습니다.\nVTSM 렌더링 해상도: VTS를 480p로 고정한 상태에서 VTSM을 480p에서 4K로 높이면 프레임률은 약 240에서 48 FPS로 떨어지고 지연 시간은 0.62에서 17.66 ms로 늘어납니다. 수신 시간은 거의 변하지 않으며, 증가분은 주로 GPU 스케일링과 Windows 투명 창 표시에서 발생합니다. 1440p 이후 부담이 뚜렷하게 커지고 4K에서는 두 단계가 주요 병목이 됩니다. GPU 사용률이 높아지면 데스크톱 합성에 배정되는 시간도 줄어듭니다. 따라서 VTS 입력 크기보다 VTSM이 처리하고 화면에 표시하는 픽셀 수가 성능을 더 직접적으로 좌우합니다.\n요약: 두 해상도가 가까울수록 불필요한 스케일링이 줄어 프레임률과 지연 시간이 안정되지만, 일치율만으로 성능이 결정되지는 않습니다. 일치율이 같아도 낮은 출력 해상도가 더 빠르며, 완전히 일치한 4K도 많은 픽셀을 처리해야 합니다. 일상 사용에는 선명도, 낮은 지연 시간과 높은 프레임률의 균형이 좋은 1280×720을 권장합니다. 세부 표현이 더 중요하다면 1920×1080을 선택할 수 있습니다. 꼭 필요하지 않다면 고해상도 출력과 스케일링을 장시간 사용하지 말고, VTS와 VTSM의 해상도를 가능한 한 가깝게 맞추세요.', latency:'총 처리 지연 시간(ms)', latencyAxis:'지연 시간(ms)', fps:'VTSFloat_Meow 실제 프레임률(FPS)', components:'출력 해상도별 지연 시간 구성', gpu:'GPU 사용률 히트맵(%)', source:'VTS 원본 해상도', render:'VTSFloat_Meow 렌더링 해상도', receive:'수신', scale:'스케일링', present:'화면 표시', programFps:'앱 프레임률(FPS)', previous:'이전 차트', next:'다음 차트', choose:'차트 선택', show:'{n}번 차트 보기' },
    ru: { datasetTitle:'Тест разрешений AMD Radeon Graphics', method:'Тестовая платформа: AMD Ryzen 7 9800X3D / DDR5-6400 CL28', caveat:'На производительность также влияют число полигонов модели VTS, эффекты, аксессуары, физика и другие запущенные приложения. При сборе данных по возможности исключались неравномерные факторы, включая остановку захвата движений.', conclusion:'Исходное разрешение VTS: Если зафиксировать VTSM на 480p и снизить разрешение VTS с 4K до 480p, частота кадров вырастает примерно со 180 до 240 FPS, а задержка сокращается с 3,04 до 0,43 мс. Приём остаётся быстрым, однако текстура высокого разрешения требует чтения большего числа пикселей и более тяжёлой фильтрации при уменьшении. Поэтому крупный холст VTS повышает нагрузку на GPU, даже если размер вывода не меняется. Разрешение источника остаётся вторичным фактором: если прирост детализации модели почти незаметен, снижение разрешения VTS поможет получить более стабильную частоту кадров.\nРазрешение рендеринга VTSM: Если зафиксировать VTS на 480p и увеличить разрешение VTSM с 480p до 4K, частота кадров снижается примерно с 240 до 48 FPS, а задержка возрастает с 0,62 до 17,66 мс. Время приёма почти не меняется. Основной прирост нагрузки создают масштабирование на GPU и вывод прозрачного окна Windows. После 1440p затраты заметно растут, а при 4K оба этапа становятся главными узкими местами. Рост загрузки GPU дополнительно ограничивает время, доступное композитору рабочего стола. Поэтому число пикселей, которые VTSM обрабатывает и выводит, влияет на производительность сильнее, чем размер входного изображения VTS.\nИтог: Чем ближе разрешения VTS и VTSM, тем меньше лишнего масштабирования и тем стабильнее частота кадров и задержка, но степень совпадения — не единственный критерий. При одинаковом совпадении более низкое разрешение вывода всё равно работает быстрее, а полностью согласованный 4K требует обработки большого числа пикселей. Для повседневной работы рекомендуется 1280×720 как баланс чёткости, низкой задержки и высокой частоты кадров; если важнее детали, подойдёт 1920×1080. Без необходимости не следует постоянно использовать высокое разрешение вывода и масштабирования, а размеры VTS и VTSM лучше держать как можно ближе друг к другу.', latency:'Общая задержка обработки (мс)', latencyAxis:'Задержка (мс)', fps:'Фактическая частота VTSFloat_Meow (FPS)', components:'Состав задержки по разрешению вывода', gpu:'Тепловая карта загрузки GPU (%)', source:'Исходное разрешение VTS', render:'Разрешение VTSFloat_Meow', receive:'Приём', scale:'Масштаб', present:'Вывод', programFps:'FPS приложения', previous:'Предыдущий график', next:'Следующий график', choose:'Выбор графика', show:'Показать график {n}' }
  };

  const alignmentTranslations = {
    'zh-CN': { alignment:'分辨率对齐率对帧率与延迟的影响', alignmentRate:'对齐率（%）', fpsAxis:'程序帧率（FPS）', nativeOrder:'程序固定 640×480 · VTS 原生分辨率由高到低', renderOrder:'VTS 固定 640×480 · 程序分辨率由低到高', aligned:'100% 对齐', videoTitle:'在常用分辨率下的延迟表现（将 VTSFloat_Meow 重叠在 VTube Studio 中）', videoSubtitle:'在 1080p 对齐的分辨率下，日常模型映射延迟几乎无感' },
    'zh-TW': { alignment:'解析度一致程度對幀率與延遲的影響', alignmentRate:'一致程度（%）', fpsAxis:'程式幀率（FPS）', nativeOrder:'程式固定 640×480 · VTS 原生解析度由高至低', renderOrder:'VTS 固定 640×480 · 程式解析度由低至高', aligned:'100% 一致', videoTitle:'常用解析度的延遲表現（將 VTSFloat_Meow 疊放於 VTube Studio 上）', videoSubtitle:'兩端皆為 1080p 時，日常模型映射的延遲幾乎難以察覺' },
    en: { alignment:'How resolution alignment affects FPS and latency', alignmentRate:'Alignment (%)', fpsAxis:'Application FPS', nativeOrder:'App fixed at 640×480 · VTS native high to low', renderOrder:'VTS fixed at 640×480 · app low to high', aligned:'100% aligned', videoTitle:'Latency at common resolutions (VTSFloat_Meow overlaid on VTube Studio)', videoSubtitle:'At aligned 1080p resolution, everyday model mapping latency is virtually imperceptible' },
    ja: { alignment:'解像度の一致率が FPS と遅延に与える影響', alignmentRate:'一致率（%）', fpsAxis:'アプリ FPS', nativeOrder:'アプリを 640×480 に固定 · VTS を高解像度から低解像度へ', renderOrder:'VTS を 640×480 に固定 · アプリを低解像度から高解像度へ', aligned:'100% 一致', videoTitle:'一般的な解像度での遅延（VTSFloat_Meow を VTube Studio に重ねて表示）', videoSubtitle:'1080p で解像度を一致させれば、日常的なモデル表示の遅延はほぼ感じられません' },
    ko: { alignment:'해상도 일치 정도가 프레임률과 지연 시간에 미치는 영향', alignmentRate:'일치 정도(%)', fpsAxis:'앱 프레임률(FPS)', nativeOrder:'앱 640×480 고정 · VTS 원본 해상도 높은 순', renderOrder:'VTS 640×480 고정 · 앱 해상도 낮은 순', aligned:'100% 일치', videoTitle:'자주 쓰는 해상도의 지연 시간(VTSFloat_Meow를 VTube Studio 위에 겹쳐 표시)', videoSubtitle:'양쪽을 1080p로 맞추면 일상적인 모델 매핑 지연은 거의 느껴지지 않습니다' },
    ru: { alignment:'Влияние совпадения разрешений на FPS и задержку', alignmentRate:'Совпадение (%)', fpsAxis:'FPS приложения', nativeOrder:'Приложение 640×480 · VTS от высокого к низкому', renderOrder:'VTS 640×480 · приложение от низкого к высокому', aligned:'Совпадение 100%', videoTitle:'Задержка при распространённых разрешениях (VTSFloat_Meow поверх VTube Studio)', videoSubtitle:'При согласованном разрешении 1080p задержка отображения модели почти незаметна' }
  };

  const slides = [...root.querySelectorAll('[data-benchmark-slide]')];
  const dots = [...root.querySelectorAll('[data-benchmark-dot]')];
  const stage = root.querySelector('.benchmark-carousel-stage');
  const previousButton = root.querySelector('.benchmark-nav-prev');
  const nextButton = root.querySelector('.benchmark-nav-next');
  const currentText = root.querySelector('[data-benchmark-current]');
  const languageSelect = document.querySelector('#language-select');
  let current = 0;
  let pointerStart = null;
  let autoSwitchTimer = null;

  const esc = (value) => String(value).replace(/[&<>\"]/g, (char) => ({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;'}[char]));
  const hex = (value) => value.replace('#', '').match(/.{2}/g).map((part) => parseInt(part, 16));
  const blend = (from, to, amount) => {
    const a = hex(from); const b = hex(to);
    return `rgb(${a.map((value, index) => Math.round(value + (b[index] - value) * amount)).join(',')})`;
  };
  const lang = () => {
    const selected = window.VTSFloatI18n?.getLocale() || languageSelect?.value;
    const code = translations[selected] ? selected : 'en';
    return {...translations[code], ...alignmentTranslations[code]};
  };

  const heatmap = (title, matrix, colors, decimals) => {
    const t = lang();
    const flat = matrix.flat(); const min = Math.min(...flat); const max = Math.max(...flat);
    const left = 158; const top = 105; const cellWidth = 98; const cellHeight = 48;
    const columns = data.render.map((label, i) => `<text x="${left + i * cellWidth + cellWidth / 2}" y="95" text-anchor="middle" class="chart-axis">${label}</text>`).join('');
    const rows = data.source.map((label, row) => {
      const y = top + row * cellHeight;
      const cells = matrix[row].map((value, col) => {
        const amount = (value - min) / (max - min || 1);
        const fill = blend(colors[0], colors[1], amount);
        const text = value.toFixed(decimals);
        return `<g><title>${esc(label)} → ${esc(data.render[col])}: ${text}</title><rect x="${left + col * cellWidth}" y="${y}" width="${cellWidth - 2}" height="${cellHeight - 2}" rx="5" fill="${fill}"/><text x="${left + col * cellWidth + cellWidth / 2 - 1}" y="${y + 29}" text-anchor="middle" class="chart-cell" fill="${amount > .58 ? '#fff' : '#17212b'}">${text}</text></g>`;
      }).join('');
      return `<text x="146" y="${y + 29}" text-anchor="end" class="chart-axis">${label}</text>${cells}`;
    }).join('');
    return `<svg viewBox="0 0 900 520" role="img" aria-label="${esc(title)}"><style>.chart-title{font:600 24px 'Segoe UI','Microsoft YaHei',sans-serif;fill:#17212b}.chart-axis{font:13px 'Segoe UI','Microsoft YaHei',sans-serif;fill:#526577}.chart-cell{font:600 13px 'Segoe UI','Microsoft YaHei',sans-serif}</style><rect width="900" height="520" fill="#fff"/><text x="36" y="45" class="chart-title">${esc(title)}</text>${columns}${rows}<text x="501" y="474" text-anchor="middle" class="chart-axis">${esc(t.render)} →</text><text x="39" y="275" text-anchor="middle" class="chart-axis" transform="rotate(-90 39 275)">${esc(t.source)} →</text></svg>`;
  };

  const lineChart = () => {
    const t = lang(); const left = 92; const top = 112; const width = 700; const height = 290; const max = 15;
    const x = (index) => left + index * width / (data.render.length - 1);
    const y = (value) => top + height - value / max * height;
    const series = [
      {label:t.receive, values:data.receive, color:'#44a3a0'},
      {label:t.scale, values:data.scale, color:'#176ca5'},
      {label:t.present, values:data.present, color:'#7bb13c'},
      {label:t.programFps, values:data.programFps.map((value) => value / 250 * max), raw:data.programFps, color:'#d46a4c', dash:'7 5'}
    ];
    const grid = [0,3,6,9,12,15].map((value) => `<line x1="${left}" y1="${y(value)}" x2="${left + width}" y2="${y(value)}" stroke="#e7edf2"/><text x="80" y="${y(value)+5}" text-anchor="end" class="chart-axis">${value}</text>`).join('');
    let legendX = 95;
    const legend = series.map((item) => { const result = `<line x1="${legendX}" y1="78" x2="${legendX+26}" y2="78" stroke="${item.color}" stroke-width="4"${item.dash ? ` stroke-dasharray="${item.dash}"` : ''}/><text x="${legendX+34}" y="83" class="chart-axis">${esc(item.label)}</text>`; legendX += item.label.length > 12 ? 230 : 112; return result; }).join('');
    const plots = series.map((item) => {
      const points = item.values.map((value, index) => `${x(index)},${y(value)}`).join(' ');
      const marks = item.values.map((value, index) => `<g><title>${data.render[index]} ${esc(item.label)}: ${(item.raw?.[index] ?? value).toFixed(item.raw ? 1 : 3)}</title><circle cx="${x(index)}" cy="${y(value)}" r="5" fill="${item.color}"/></g>`).join('');
      return `<polyline points="${points}" fill="none" stroke="${item.color}" stroke-width="3"${item.dash ? ` stroke-dasharray="${item.dash}"` : ''}/>${marks}`;
    }).join('');
    const labels = data.render.map((label, index) => `<text x="${x(index)}" y="430" text-anchor="end" class="chart-axis" transform="rotate(-28 ${x(index)} 430)">${label}</text>`).join('');
    const fpsY = (value) => y(value / 250 * max);
    const fpsTicks = [0,50,100,150,200,250].map((value) => `<line x1="820" y1="${fpsY(value)}" x2="828" y2="${fpsY(value)}" stroke="#d46a4c"/><text x="834" y="${fpsY(value)+5}" class="chart-axis chart-fps">${value}</text>`).join('');
    return `<svg viewBox="0 0 900 520" role="img" aria-label="${esc(t.components)}"><style>.chart-title{font:600 24px 'Segoe UI','Microsoft YaHei',sans-serif;fill:#17212b}.chart-axis{font:13px 'Segoe UI','Microsoft YaHei',sans-serif;fill:#526577}.chart-fps{fill:#b95339}</style><rect width="900" height="520" fill="#fff"/><text x="36" y="45" class="chart-title">${esc(t.components)}</text>${legend}${grid}${plots}${labels}<text x="34" y="270" text-anchor="middle" class="chart-axis" transform="rotate(-90 34 270)">${esc(t.latencyAxis)}</text><line x1="820" y1="${top}" x2="820" y2="${top+height}" stroke="#d46a4c" stroke-opacity=".65"/>${fpsTicks}<text x="875" y="257" text-anchor="middle" class="chart-axis chart-fps" transform="rotate(90 875 257)">${esc(t.programFps)}</text></svg>`;
  };

  const alignmentChart = () => {
    const t = lang();
    const left = 72; const top = 118; const width = 690; const height = 250; const bottom = top + height;
    const step = width / (alignmentData.labels.length - 1);
    const x = (index) => left + index * step;
    const yAlignment = (value) => bottom - value / 100 * height;
    const yFps = (value) => bottom - value / 250 * height;
    const yLatency = (value) => bottom - value / 20 * height;
    const grid = [0,50,100,150,200,250].map((value) => `<line x1="${left}" y1="${yFps(value)}" x2="${left + width}" y2="${yFps(value)}" stroke="#e7edf2"/>`).join('');
    const bars = alignmentData.alignment.map((value, index) => {
      const barWidth = 28; const barX = x(index) - barWidth / 2; const barY = yAlignment(value);
      return `<g><title>${alignmentData.labels[index]} · ${t.alignmentRate}: ${value.toFixed(1)}%</title><rect x="${barX}" y="${barY}" width="${barWidth}" height="${bottom - barY}" rx="4" fill="${index === 6 ? '#7770b5' : '#c8c4ef'}"/><text x="${x(index)}" y="${Math.max(top + 12, barY - 7)}" text-anchor="middle" class="chart-value">${Math.round(value)}</text></g>`;
    }).join('');
    const fpsPoints = alignmentData.fps.map((value, index) => `${x(index)},${yFps(value)}`).join(' ');
    const latencyPoints = alignmentData.latency.map((value, index) => `${x(index)},${yLatency(value)}`).join(' ');
    const fpsMarks = alignmentData.fps.map((value, index) => `<g><title>${alignmentData.labels[index]} · ${t.fpsAxis}: ${value.toFixed(1)}</title><circle cx="${x(index)}" cy="${yFps(value)}" r="4" fill="#d46a4c"/></g>`).join('');
    const latencyMarks = alignmentData.latency.map((value, index) => `<g><title>${alignmentData.labels[index]} · ${t.latencyAxis}: ${value.toFixed(2)}</title><circle cx="${x(index)}" cy="${yLatency(value)}" r="4" fill="#176ca5"/></g>`).join('');
    const labels = alignmentData.labels.map((label, index) => `<text x="${x(index)}" y="390" text-anchor="end" class="chart-axis chart-x" transform="rotate(-46 ${x(index)} 390)">${label}</text>`).join('');
    const fpsTicks = [0,50,100,150,200,250].map((value) => `<line x1="${left-7}" y1="${yFps(value)}" x2="${left}" y2="${yFps(value)}" stroke="#d46a4c"/><text x="${left-12}" y="${yFps(value)+4}" text-anchor="end" class="chart-axis chart-fps">${value}</text>`).join('');
    const latencyTicks = [0,5,10,15,20].map((value) => `<line x1="822" y1="${yLatency(value)}" x2="829" y2="${yLatency(value)}" stroke="#176ca5"/><text x="834" y="${yLatency(value)+4}" class="chart-axis chart-latency">${value}</text>`).join('');
    return `<svg viewBox="0 0 900 520" role="img" aria-label="${esc(t.alignment)}"><style>.chart-title{font:600 23px 'Segoe UI','Microsoft YaHei',sans-serif;fill:#17212b}.chart-axis{font:12px 'Segoe UI','Microsoft YaHei',sans-serif;fill:#526577}.chart-value{font:600 11px 'Segoe UI','Microsoft YaHei',sans-serif;fill:#575273}.chart-fps{fill:#b95339}.chart-latency{fill:#176ca5}.chart-group{font:12px 'Segoe UI','Microsoft YaHei',sans-serif;fill:#7770b5}</style><rect width="900" height="520" fill="#fff"/><text x="34" y="42" class="chart-title">${esc(t.alignment)}</text><line x1="80" y1="72" x2="105" y2="72" stroke="#c8c4ef" stroke-width="10"/><text x="113" y="77" class="chart-axis">${esc(t.alignmentRate)}</text><line x1="270" y1="72" x2="295" y2="72" stroke="#176ca5" stroke-width="3"/><text x="303" y="77" class="chart-axis">${esc(t.latencyAxis)}</text>${grid}${bars}<polyline points="${fpsPoints}" fill="none" stroke="#d46a4c" stroke-width="3"/>${fpsMarks}<polyline points="${latencyPoints}" fill="none" stroke="#176ca5" stroke-width="3"/>${latencyMarks}${labels}<line x1="${left}" y1="${top}" x2="${left}" y2="${bottom}" stroke="#d46a4c" stroke-opacity=".55"/>${fpsTicks}<line x1="822" y1="${top}" x2="822" y2="${bottom}" stroke="#176ca5" stroke-opacity=".55"/>${latencyTicks}<text x="20" y="252" text-anchor="middle" class="chart-axis chart-fps" transform="rotate(-90 20 252)">${esc(t.fpsAxis)}</text><text x="860" y="252" text-anchor="middle" class="chart-axis chart-latency" transform="rotate(90 860 252)">${esc(t.latencyAxis)}</text><text x="244" y="488" text-anchor="middle" class="chart-group">${esc(t.nativeOrder)}</text><text x="657" y="488" text-anchor="middle" class="chart-group">${esc(t.renderOrder)}</text><line x1="417" y1="96" x2="417" y2="368" stroke="#7770b5" stroke-width="1.5" stroke-dasharray="5 5"/><text x="417" y="106" text-anchor="middle" class="chart-group">${esc(t.aligned)}</text></svg>`;
  };

  const renderCharts = () => {
    const t = lang();
    root.querySelector('[data-benchmark-text="datasetTitle"]').textContent = t.datasetTitle;
    root.querySelector('[data-benchmark-text="method"]').textContent = t.method;
    root.querySelector('[data-benchmark-text="caveat"]').textContent = t.caveat;
    const videoTitle = document.querySelector('[data-benchmark-text="videoTitle"]');
    const videoSubtitle = document.querySelector('[data-benchmark-text="videoSubtitle"]');
    const benchmarkVideo = document.querySelector('.benchmark-video');
    videoTitle.textContent = t.videoTitle;
    videoSubtitle.textContent = t.videoSubtitle;
    benchmarkVideo.setAttribute('aria-label', t.videoTitle);
    const conclusion = document.querySelector('[data-benchmark-text="conclusion"]');
    conclusion.replaceChildren(...t.conclusion.split('\n').map((line) => {
      const paragraph = document.createElement('p');
      const separatorIndex = Math.max(line.indexOf('：'), line.indexOf(':'));
      if (separatorIndex < 0) {
        paragraph.textContent = line;
        return paragraph;
      }
      const label = document.createElement('strong');
      label.textContent = line.slice(0, separatorIndex + 1);
      paragraph.append(label, line.slice(separatorIndex + 1));
      return paragraph;
    }));
    root.querySelector('[data-benchmark-chart="latency"]').innerHTML = heatmap(t.latency, data.totalLatency, ['#edf6fc','#176ca5'], 2);
    root.querySelector('[data-benchmark-chart="fps"]').innerHTML = heatmap(t.fps, data.fps, ['#edf6e7','#347d1f'], 0);
    root.querySelector('[data-benchmark-chart="components"]').innerHTML = lineChart();
    root.querySelector('[data-benchmark-chart="gpu"]').innerHTML = heatmap(t.gpu, data.gpu, ['#fff4dc','#b54a2a'], 1);
    root.querySelector('[data-benchmark-chart="alignment"]').innerHTML = alignmentChart();
    root.setAttribute('aria-label', t.datasetTitle);
    previousButton.setAttribute('aria-label', t.previous); nextButton.setAttribute('aria-label', t.next);
    root.querySelector('.benchmark-dots').setAttribute('aria-label', t.choose);
    dots.forEach((dot, index) => dot.setAttribute('aria-label', t.show.replace('{n}', index + 1)));
  };

  const show = (index) => {
    current = (index + slides.length) % slides.length;
    const previous = (current - 1 + slides.length) % slides.length;
    const next = (current + 1) % slides.length;
    slides.forEach((slide, index) => {
      slide.classList.toggle('is-active', index === current);
      slide.classList.toggle('is-prev', index === previous);
      slide.classList.toggle('is-next', index === next);
      slide.setAttribute('aria-hidden', String(index !== current));
    });
    dots.forEach((dot, index) => dot.classList.toggle('is-active', index === current));
    currentText.textContent = current + 1;
  };

  const pauseAutoSwitch = () => {
    window.clearTimeout(autoSwitchTimer);
    autoSwitchTimer = null;
  };

  const scheduleAutoSwitch = () => {
    pauseAutoSwitch();
    if (document.hidden || root.matches(':hover')) return;
    autoSwitchTimer = window.setTimeout(() => {
      show(current + 1);
      scheduleAutoSwitch();
    }, 2800);
  };

  previousButton.addEventListener('click', () => show(current - 1));
  nextButton.addEventListener('click', () => show(current + 1));
  slides.forEach((slide, index) => slide.addEventListener('click', () => { if (index !== current) show(index); }));
  dots.forEach((dot, index) => dot.addEventListener('click', () => show(index)));
  stage.addEventListener('keydown', (event) => { if (event.key === 'ArrowLeft') show(current - 1); if (event.key === 'ArrowRight') show(current + 1); });
  stage.addEventListener('pointerdown', (event) => { pointerStart = event.clientX; });
  stage.addEventListener('pointerup', (event) => { if (pointerStart === null) return; const distance = event.clientX - pointerStart; pointerStart = null; if (Math.abs(distance) > 55) show(current + (distance < 0 ? 1 : -1)); });
  stage.addEventListener('pointercancel', () => { pointerStart = null; });
  root.addEventListener('mouseenter', pauseAutoSwitch);
  root.addEventListener('mouseleave', scheduleAutoSwitch);
  document.addEventListener('visibilitychange', scheduleAutoSwitch);
  window.addEventListener('vtsfloat:languagechange', renderCharts);

  renderCharts();
  show(0);
  scheduleAutoSwitch();
})();
