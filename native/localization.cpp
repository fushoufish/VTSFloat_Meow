#include "localization.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <windows.h>

namespace vtsfloat::i18n {
namespace {

struct Translation {
    const wchar_t* zh;
    const wchar_t* en;
    const wchar_t* ja;
    const wchar_t* ko;
    const wchar_t* ru;
};

// Terminology follows the VTube Studio UI/API where it has an established
// name (Spout2, Plugins API, expression), and normal desktop-app language
// elsewhere. These are authored translations, not runtime machine output.
constexpr Translation kTranslations[] = {
    {L"：", L": ", L"：", L": ", L": "},
    {L"图形设置", L"Graphics", L"グラフィック", L"그래픽", L"Графика"},
    {L"悬浮窗口", L"Floating", L"フロート", L"플로팅", L"Поверх"},
    {L"桌面模式", L"Desktop", L"デスクトップ", L"데스크톱", L"На столе"},
    {L"桌面模式：其他程序全屏时暂停输出", L"Desktop mode: pause output while another app is fullscreen", L"デスクトップ：他のアプリが全画面の間は出力を一時停止", L"데스크톱 모드: 다른 앱이 전체 화면이면 출력 일시 정지", L"Режим рабочего стола: приостанавливать вывод, когда другое приложение на весь экран"},
    {L"切换屏幕", L"Next", L"画面切替", L"화면 전환", L"Экран"},
    {L"锁定/解锁 ", L"Lock/unlock ", L"ロック切替 ", L"잠금 전환 ", L"Блокировка "},
    {L"请按组合键（Esc取消）", L"Press shortcut (Esc to cancel)", L"ショートカットを入力（Escで取消）", L"단축키 입력(Esc: 취소)", L"Нажмите сочетание (Esc — отмена)"},
    {L"比例 锁定", L"Aspect on", L"比率固定", L"비율 잠금", L"Фикс."},
    {L"比例 自由", L"Free", L"比率自由", L"비율 자유", L"Своб."},
    {L"个性化", L"Style", L"カスタム", L"꾸미기", L"Вид"},
    {L"调试", L"Debug", L"デバッグ", L"디버그", L"Отладка"},
    {L"完成", L"Done", L"完了", L"완료", L"Готово"},
    {L"解锁", L"Unlock", L"ロック解除", L"잠금 해제", L"Разблокировать"},
    {L"调试模式已打开", L"Debug mode is on", L"デバッグモード：オン", L"디버그 모드 켜짐", L"Режим отладки включён"},
    {L"清除缓存并重置脚本", L"Clear settings and reset", L"設定を消去してリセット", L"설정 삭제 및 초기화", L"Очистить настройки и сбросить"},
    {L"推荐（核显/节能 GPU）", L"Recommended (integrated/power-saving GPU)", L"推奨（内蔵／省電力 GPU）", L"권장(내장/절전 GPU)", L"Рекомендуется (встроенный/энергосберегающий GPU)"},
    {L"GPU 选择", L"GPU selection", L"GPU 選択", L"GPU 선택", L"Выбор GPU"},
    {L"性能（最近邻）", L"Performance (nearest-neighbor)", L"パフォーマンス（最近傍）", L"성능(최근접)", L"Производительность (по соседнему)"},
    {L"平衡（双线性，推荐）", L"Balanced (bilinear, recommended)", L"バランス（バイリニア、推奨）", L"균형(이중 선형, 권장)", L"Баланс (билинейная, рекомендуется)"},
    {L"质量（双三次）", L"Quality (bicubic)", L"品質（バイキュービック）", L"품질(바이큐빅)", L"Качество (бикубическая)"},
    {L"画质", L"Scaling quality", L"拡大縮小品質", L"스케일링 품질", L"Качество масштабирования"},
    {L"对齐 VTS 实际渲染帧数（推荐）", L"Match actual VTS render FPS (recommended)", L"VTS の実レンダリング FPS に同期（推奨）", L"실제 VTS 렌더링 FPS에 맞춤(권장)", L"По фактической частоте VTS (рекомендуется)"},
    {L"对齐 VTS 实际渲染帧数（未启用 API）", L"Match actual VTS render FPS (API unavailable)", L"VTS の実レンダリング FPS に同期（API 未接続）", L"실제 VTS 렌더링 FPS에 맞춤(API 연결 안 됨)", L"По фактической частоте VTS (API недоступен)"},
    {L"跟随 VTS 配置", L"Follow VTS settings", L"VTS 設定に合わせる", L"VTS 설정 따르기", L"Следовать настройкам VTS"},
    {L"自定义…（1-240）", L"Custom… (1–240)", L"カスタム…（1～240）", L"사용자 지정…(1~240)", L"Вручную… (1–240)"},
    {L"帧率", L"Frame rate", L"フレームレート", L"프레임 속도", L"Частота кадров"},
    {L"性能（最近邻，最低开销）", L"Performance (nearest-neighbor, lowest cost)", L"パフォーマンス（最近傍、最小負荷）", L"성능(최근접, 최소 부하)", L"Производительность (по соседнему, минимум нагрузки)"},
    {L"平衡（GPU 双线性，推荐）", L"Balanced (GPU bilinear, recommended)", L"バランス（GPU バイリニア、推奨）", L"균형(GPU 이중 선형, 권장)", L"Баланс (билинейная GPU, рекомендуется)"},
    {L"质量（GPU 双三次，更细腻）", L"Quality (GPU bicubic, finer edges)", L"品質（GPU バイキュービック、精細）", L"품질(GPU 바이큐빅, 더 세밀함)", L"Качество (бикубическая GPU, чётче)"},
    {L"如何开启 VTubeStudio Plugins API", L"How to enable the VTube Studio Plugins API", L"VTube Studio Plugins API の有効化方法", L"VTube Studio Plugins API 활성화 방법", L"Как включить VTube Studio Plugins API"},
    {L"1. 打开 VTube Studio 设置。\n2. 在“常规设置”中启用“允许插件 API 访问”。\n3. 返回 VTSFloat_Meow，点击“扫描端口”。\n4. VTube Studio 弹出授权请求时，选择允许。", L"1. Open VTube Studio Settings.\n2. Under General Settings, enable “Allow Plugin API access”.\n3. Return to VTSFloat_Meow and select “Scan ports”.\n4. When VTube Studio asks for authorization, select Allow.", L"1. VTube Studio の設定を開きます。\n2. 「一般設定」で「プラグイン API へのアクセスを許可」を有効にします。\n3. VTSFloat_Meow に戻り、「ポートを検索」を選択します。\n4. VTube Studio に承認リクエストが表示されたら、許可します。", L"1. VTube Studio 설정을 엽니다.\n2. 일반 설정에서 ‘플러그인 API 접근 허용’을 켭니다.\n3. VTSFloat_Meow로 돌아가 ‘포트 검색’을 선택합니다.\n4. VTube Studio에 승인 요청이 표시되면 허용을 선택합니다.", L"1. Откройте настройки VTube Studio.\n2. В общих настройках включите «Разрешить доступ к Plugin API».\n3. Вернитесь в VTSFloat_Meow и выберите «Сканировать порты».\n4. Когда VTube Studio запросит авторизацию, нажмите «Разрешить»."},
    {L"无法打开自定义帧率输入框。", L"Could not open the custom frame-rate input.", L"カスタムフレームレート入力を開けませんでした。", L"사용자 지정 프레임 속도 입력을 열 수 없습니다.", L"Не удалось открыть ввод частоты кадров."},
    {L"请输入 1 到 240 之间的整数。", L"Enter a whole number from 1 to 240.", L"1～240 の整数を入力してください。", L"1~240 사이의 정수를 입력하세요.", L"Введите целое число от 1 до 240."},
    {L"帧率无效", L"Invalid frame rate", L"無効なフレームレート", L"잘못된 프레임 속도", L"Недопустимая частота кадров"},
    {L"普通渐变（整圈同步变色）", L"Standard gradient (whole border)", L"通常グラデーション（全周同期）", L"일반 그라데이션(전체 테두리)", L"Обычный градиент (вся рамка)"},
    {L"彩虹跑马灯", L"Rainbow chase", L"レインボーチェイス", L"무지개 추적 효과", L"Бегущая радуга"},
    {L"起始颜色", L"Start color", L"開始色", L"시작 색상", L"Начальный цвет"},
    {L"结束颜色", L"End color", L"終了色", L"끝 색상", L"Конечный цвет"},
    {L"边框模式", L"Border", L"枠線", L"테두리", L"Рамка"},
    {L"自定义", L"Solid color", L"単色", L"단색", L"Один цвет"},
    {L"跑马灯渐变", L"Chase", L"チェイス", L"추적", L"Бегущий"},
    {L"普通渐变", L"Gradient", L"グラデーション", L"그라데이션", L"Градиент"},
    {L"自定义颜色", L"Custom color", L"カスタムカラー", L"사용자 지정 색상", L"Свой цвет"},
    {L"亮度", L"Brightness", L"明るさ", L"밝기", L"Яркость"},
    {L"边框粗细", L"Border width", L"枠線の太さ", L"테두리 두께", L"Толщина"},
    {L"模型不透明度", L"Model opacity", L"モデルの不透明度", L"모델 불투명도", L"Непрозрачность"},
    {L"界面缩放", L"UI scale", L"UIスケール", L"UI 크기", L"Масштаб UI"},
    {L"鼠标经过模型时将模型透明化", L"Fade the model when the pointer is over it", L"ポインターがモデル上にある間、モデルを透過", L"포인터가 모델 위에 있을 때 모델 투명화", L"Делать модель прозрачнее при наведении"},
    {L"悬停不透明度", L"Hover opacity", L"ホバー時の不透明度", L"호버 불투명도", L"При наведении"},
    {L"透明度恢复延时", L"Opacity restore delay", L"不透明度の復元待ち時間", L"불투명도 복원 지연", L"Задержка возврата"},
    {L"默认 0 秒", L"Default: 0 sec", L"既定：0秒", L"기본값: 0초", L"По умолч.: 0 с"},
    {L"悬停扩展", L"Hover range", L"ホバー範囲", L"호버 범위", L"Зона"},
    {L"启用手动框选范围", L"Manual regions", L"手動選択範囲を使用", L"수동 영역 사용", L"Ручные области"},
    {L"手动框选悬停触发范围", L"Select hover trigger regions", L"ホバー判定範囲を選択", L"가리키기 감지 영역 선택", L"Выбрать области"},
    {L"鼠标移入时触发表情，移出后立即恢复", L"Trigger expressions on hover", L"ホバーで表情を再生し、離れた後に復元", L"호버 시 표정 재생", L"Эмоции при наведении"},
    {L"表情恢复延时", L"Restore delay", L"復元まで", L"복원 지연", L"Задержка"},
    {L"秒", L"sec", L"秒", L"초", L"с"},
    {L"ENTER 确认", L"ENTER to confirm", L"ENTER で確定", L"ENTER로 확인", L"ENTER: ОК"},
    {L"选择表情", L"Select expressions", L"表情を選択", L"표정 선택", L"Выбрать эмоции"},
    {L"表情：", L"Expressions: ", L"表情：", L"표정: ", L"Эмоции: "},
    {L"已选择 ", L"Selected ", L"選択済み：", L"선택됨: ", L"Выбрано: "},
    {L" 个表情", L" expressions", L" 件", L"개", L""},
    {L"恢复默认", L"Defaults", L"既定値", L"기본값", L"Сбросить"},
    {L"取消", L"Cancel", L"キャンセル", L"취소", L"Отмена"},
    {L"暂无可用表情，请确认 VTS API 已连接", L"No expressions available. Check the VTS API connection.", L"利用可能な表情がありません。VTS API の接続を確認してください。", L"사용 가능한 표정이 없습니다. VTS API 연결을 확인하세요.", L"Нет доступных эмоций. Проверьте подключение к VTS API."},
    {L"未命名表情", L"Unnamed expression", L"名称未設定の表情", L"이름 없는 표정", L"Эмоция без имени"},
    {L"选择完成", L"Finish selection", L"選択を完了", L"선택 완료", L"Готово"},
    {L"已选 ", L"Selected: ", L"選択：", L"선택: ", L"Выбрано: "},
    {L" 项", L"", L" 件", L"개", L""},
    {L"手动框选悬停范围（支持多选）", L"Select hover regions (multiple regions supported)", L"ホバー範囲を選択（複数可）", L"가리키기 영역 선택(여러 영역 가능)", L"Выберите области наведения (можно несколько)"},
    {L"逐点点击勾勒范围；双击或点击起点完成一个范围，然后可继续框选。回车完成全部，Esc 取消。", L"Click to outline a region. Double-click or click the first point to close it, then continue with another. Enter finishes; Esc cancels.", L"クリックして範囲を囲みます。ダブルクリックまたは始点をクリックして1つの範囲を閉じ、続けて追加できます。Enterで完了、Escでキャンセル。", L"클릭하여 영역을 그립니다. 두 번 클릭하거나 시작점을 클릭해 한 영역을 닫은 뒤 계속 추가할 수 있습니다. Enter: 완료, Esc: 취소.", L"Щёлкайте по контуру области. Двойной щелчок или щелчок по первой точке замыкает её; затем можно добавить ещё. Enter — готово, Esc — отмена."},
    {L"当前框选主体数量：", L"Selected regions: ", L"選択済み範囲：", L"선택한 영역: ", L"Выбрано областей: "},
    {L"完成  Enter", L"Done  Enter", L"完了  Enter", L"완료  Enter", L"Готово  Enter"},
    {L"核显/节能", L"Integrated / power saving", L"内蔵／省電力", L"내장 / 절전", L"Встроенный / энергосберегающий"},
    {L"高性能", L"High performance", L"高パフォーマンス", L"고성능", L"Высокая производительность"},
    {L"Windows 自动", L"Let Windows decide", L"Windows に任せる", L"Windows에서 결정", L"Выбор Windows"},
    {L"从 Steam 启动 VTube Studio", L"Launch VTube Studio from Steam", L"Steam から VTube Studio を起動", L"Steam에서 VTube Studio 실행", L"Запустить через Steam"},
    {L"从外部启动 VTS", L"Launch VTS externally", L"外部から VTS を起動", L"외부에서 VTS 실행", L"Запустить VTS отдельно"},
    {L"当前已经在使用这个 GPU，无需重启。", L"This GPU is already in use. No restart is needed.", L"この GPU は既に使用中です。再起動は不要です。", L"이미 이 GPU를 사용 중입니다. 다시 시작할 필요가 없습니다.", L"Этот GPU уже используется. Перезапуск не требуется."},
    {L"检测到当前 Spout 非透明推流：", L"The current Spout output is not transparent:", L"現在の Spout 出力は透明ではありません：", L"현재 Spout 출력이 투명하지 않습니다:", L"Текущий вывод Spout непрозрачен:"},
    {L"[不再提示]", L"[Don't show again]", L"[今後表示しない]", L"[다시 표시 안 함]", L"[Больше не показывать]"},
    {L"[关闭提示]", L"[Dismiss]", L"[閉じる]", L"[닫기]", L"[Закрыть]"},
    {L"当前运行在高性能显卡中，高负载场景性能将会受限", L"High-performance GPU: stutter may occur under heavy load", L"高性能 GPU：高負荷時にカクつく場合があります", L"고성능 GPU: 부하가 높으면 끊길 수 있습니다", L"Высокопроизводительный GPU: возможны рывки"},
    {L"未启用", L"Not enabled", L"未有効", L"사용 안 함", L"Не включено"},
    {L"读取失败", L"Failed", L"取得失敗", L"읽기 실패", L"Ошибка"},
    {L"VTS API 未连接", L"VTS API not connected", L"VTS API 未接続", L"VTS API 연결 안 됨", L"VTS API не подключён"},
    {L"FPS / 帧时", L"FPS / frame time", L"FPS／フレーム時間", L"FPS / 프레임 시간", L"FPS / время кадра"},
    {L"已找到 VTube Studio，但当前尚未运行", L"VTube Studio was found but is not running", L"VTube Studio は見つかりましたが、起動していません", L"VTube Studio를 찾았지만 실행 중이 아닙니다", L"VTube Studio найден, но не запущен"},
    {L"等待 VTube Studio 启动中", L"Waiting for VTube Studio to start", L"VTube Studio の起動を待っています", L"VTube Studio 시작 대기 중", L"Ожидание запуска VTube Studio"},
    {L"如已启动，请在 设置 - 相机 中打开“激活 Spout2”开关，\r\n", L"If it is running, open Settings > Camera and enable “Activate Spout2”,\r\n", L"起動済みの場合は、設定 > カメラで「Spout2を有効化」をオンにし、\r\n", L"실행 중이면 설정 > 카메라에서 ‘Spout2 활성화’를 켜고,\r\n", L"Если программа запущена, откройте Настройки > Камера и включите «Активировать Spout2»,\r\n"},
    {L"将背景调整成“ColorPicker”，然后启动透明推流。", L"set the background to “ColorPicker”, then start transparent streaming.", L"背景を「ColorPicker」にして透明配信を開始してください。", L"배경을 ‘ColorPicker’로 설정한 다음 투명 스트리밍을 시작하세요.", L"выберите фон «ColorPicker» и запустите прозрачную трансляцию."},
    {L"你可以访问该教程页面：", L"Visit the tutorial: ", L"チュートリアルはこちら：", L"튜토리얼 보기: ", L"Руководство: "},
    {L"GitHub-VTSFloat_Meow_DOCS", L"GitHub-VTSFloat_Meow_DOCS", L"GitHub-VTSFloat_Meow_DOCS", L"GitHub-VTSFloat_Meow_DOCS", L"GitHub-VTSFloat_Meow_DOCS"},
    {L" 页面查看更多", L" for more information.", L" で詳しく確認できます。", L"에서 자세히 확인하세요.", L" — подробнее."},
    {L"所选位置未找到 VTube Studio，请重新选择路径。", L"VTube Studio was not found at the selected location. Choose another path.", L"選択した場所に VTube Studio が見つかりません。別の場所を選択してください。", L"선택한 위치에서 VTube Studio를 찾을 수 없습니다. 다른 경로를 선택하세요.", L"VTube Studio не найден в выбранном месте. Выберите другой путь."},
    {L"尚未找到 VTube Studio 安装目录，请先安装或启动一次 VTube Studio。", L"The VTube Studio installation was not found. Install or launch it once first.", L"VTube Studio のインストール先が見つかりません。先にインストールするか、一度起動してください。", L"VTube Studio 설치 경로를 찾을 수 없습니다. 먼저 설치하거나 한 번 실행하세요.", L"Папка VTube Studio не найдена. Сначала установите или один раз запустите программу."},
    {L"VTS 安装路径：", L"VTS installation: ", L"VTS インストール先：", L"VTS 설치 경로: ", L"Папка VTS: "},
    {L"手动选择路径", L"Choose path manually", L"手動で選択", L"경로 직접 선택", L"Выбрать путь вручную"},
    {L"添加启动路径", L"Add launch path", L"起動パスを追加", L"실행 경로 추가", L"Добавить путь запуска"},
    {L"添加成功！", L"Connected successfully!", L"接続しました！", L"연결되었습니다!", L"Подключено!"},
    {L"等待 VTube Studio 授权插件连接中，请打开 VTS 插件进行授权", L"Waiting for VTube Studio authorization. Allow the plugin in VTS", L"VTube Studio の承認待ちです。VTS でプラグインを許可してください", L"VTube Studio 승인 대기 중입니다. VTS에서 플러그인을 허용하세요", L"Ожидание разрешения VTube Studio. Разрешите плагин в VTS"},
    {L"你刚刚拒绝了VTSFloat_Meow", L"You just denied VTSFloat_Meow access", L"VTSFloat_Meow のアクセスを拒否しました", L"VTSFloat_Meow 접근을 거부했습니다", L"Вы отклонили доступ для VTSFloat_Meow"},
    {L"[重新发起授权]", L"[Request access again]", L"[承認を再リクエスト]", L"[승인 다시 요청]", L"[Запросить доступ снова]"},
    {L"VTS API 连接已断开", L"VTS API disconnected", L"VTS API が切断されました", L"VTS API 연결 끊김", L"VTS API отключён"},
    {L"VTS API 尚未授权", L"VTS API not authorized", L"VTS API が未承認です", L"VTS API 승인 필요", L"VTS API не разрешён"},
    {L"扫描中", L"Scanning", L"スキャン中", L"검색 중", L"Сканирование"},
    {L"扫描成功", L"Found", L"検出しました", L"찾음", L"Найдено"},
    {L"获取失败", L"Not found", L"見つかりません", L"찾지 못함", L"Не найдено"},
    {L"[扫描端口]", L"[Scan ports]", L"[ポートを検索]", L"[포트 검색]", L"[Сканировать порты]"},
    {L"[如何开启]", L"[Setup help]", L"[設定方法]", L"[설정 도움말]", L"[Как настроить]"},
    {L"直接输入 1–240；Enter 确认，Esc 取消（确认前不会修改当前帧率）", L"Type 1–240; Enter confirms, Esc cancels (the current rate is unchanged until confirmed)", L"1～240を入力。Enterで確定、Escで取消（確定するまで現在値は変更されません）", L"1~240 입력; Enter: 확인, Esc: 취소(확인 전에는 현재 값 유지)", L"Введите 1–240; Enter — применить, Esc — отменить (до подтверждения значение не меняется)"},
    {L"这个快捷键已被其他程序占用，请换一个组合。", L"That shortcut is already used by another app. Choose a different combination.", L"このショートカットは別のアプリで使用中です。別の組み合わせを選んでください。", L"다른 앱에서 사용 중인 단축키입니다. 다른 조합을 선택하세요.", L"Это сочетание уже используется другой программой. Выберите другое."},
    {L"重置到主屏幕中间", L"Center on primary display", L"メイン画面の中央に配置", L"주 화면 가운데로 이동", L"По центру основного экрана"},
    {L"暂时隐藏", L"Hide temporarily", L"一時的に非表示", L"일시적으로 숨기기", L"Временно скрыть"},
    {L"显示窗口", L"Show window", L"ウィンドウを表示", L"창 표시", L"Показать окно"},
    {L"退出程序", L"Exit", L"終了", L"종료", L"Выход"},
    {L"GPU 推荐（未检测到核显）", L"GPU recommendation (no integrated GPU found)", L"GPU 推奨（内蔵 GPU が見つかりません）", L"GPU 권장(내장 GPU를 찾지 못함)", L"Рекомендация GPU (встроенный GPU не найден)"},
    {L"GPU 已选：", L"Selected GPU: ", L"選択中の GPU：", L"선택한 GPU: ", L"Выбранный GPU: "},
    {L"（当前回退）", L" (currently using fallback)", L"（現在はフォールバック）", L"(현재 대체 GPU 사용 중)", L" (сейчас используется резервный)"},
    {L"选择 VTube Studio 安装文件夹", L"Select the VTube Studio installation folder", L"VTube Studio のインストールフォルダーを選択", L"VTube Studio 설치 폴더 선택", L"Выберите папку установки VTube Studio"},
    {L"可执行文件", L"Executable files", L"実行ファイル", L"실행 파일", L"Исполняемые файлы"},
    {L"选择 VTube Studio.exe", L"Select VTube Studio.exe", L"VTube Studio.exe を選択", L"VTube Studio.exe 선택", L"Выберите VTube Studio.exe"},
    {L"选择安装文件夹", L"Select installation folder", L"インストールフォルダーを選択", L"설치 폴더 선택", L"Выбрать папку установки"},
    {L"添加 VTube Studio 启动路径", L"Add a VTube Studio launch path", L"VTube Studio の起動パスを追加", L"VTube Studio 실행 경로 추가", L"Добавить путь запуска VTube Studio"},
    {L"可以选择 VTube Studio 的安装文件夹，或直接选择 VTube Studio.exe。\n程序会自动查找并保存正确的安装目录。", L"Choose the VTube Studio installation folder or VTube Studio.exe itself.\nThe correct installation directory will be found and saved automatically.", L"VTube Studio のインストールフォルダーまたは VTube Studio.exe を選択してください。\n正しいインストール先を自動的に検出して保存します。", L"VTube Studio 설치 폴더 또는 VTube Studio.exe를 선택하세요.\n올바른 설치 경로를 자동으로 찾아 저장합니다.", L"Выберите папку установки VTube Studio или сам файл VTube Studio.exe.\nПравильная папка установки будет найдена и сохранена автоматически."},
    {L"未知 GPU", L"Unknown GPU", L"不明な GPU", L"알 수 없는 GPU", L"Неизвестный GPU"},
    {L"GPU 推荐：", L"Recommended GPU: ", L"推奨 GPU：", L"권장 GPU: ", L"Рекомендуемый GPU: "},
    {L"输入 FPS：_", L"Enter FPS: _", L"FPS を入力：_", L"FPS 입력: _", L"Введите FPS: _"},
    {L"输入 FPS：", L"Enter FPS: ", L"FPS を入力：", L"FPS 입력: ", L"Введите FPS: "},
    {L"对齐 VTS 渲染", L"Match VTS rendering", L"VTS レンダリングに同期", L"VTS 렌더링에 맞춤", L"По рендерингу VTS"},
    {L"跟随屏幕 ", L"Follow display ", L"画面に合わせる ", L"화면 주사율 따르기 ", L"По частоте экрана "},
    {L"画质 性能", L"Scaling: performance", L"画質：パフォーマンス", L"화질: 성능", L"Масштабирование: скорость"},
    {L"画质 质量", L"Scaling: quality", L"画質：品質", L"화질: 품질", L"Масштабирование: качество"},
    {L"画质 平衡", L"Scaling: balanced", L"画質：バランス", L"화질: 균형", L"Масштабирование: баланс"},
    {L"请在 VTS 主界面更改背景为 \"ColorPicker\" 并启用 \"透明推流\"", L"In VTS, use “ColorPicker” and enable transparent capture", L"VTSで「ColorPicker」と「キャプチャを透過」を有効にしてください", L"VTS에서 ‘ColorPicker’와 ‘캡처 시 투명’을 켜세요", L"В VTS выберите «ColorPicker» и включите прозрачный захват"},
    {L"推荐（使用核显/节能 GPU）", L"Recommended (use integrated/power-saving GPU)", L"推奨（内蔵／省電力 GPU を使用）", L"권장(내장/절전 GPU 사용)", L"Рекомендуется (встроенный/энергосберегающий GPU)"},
    {L"（未检测到）", L" (not detected)", L"（未検出）", L"(감지되지 않음)", L" (не обнаружен)"},
    {L"建议：优先使用核显/节能 GPU 驱动覆盖层（若可用）", L"Tip: use an integrated/power-saving GPU for the overlay when available", L"推奨：利用可能な場合は内蔵／省電力 GPU でオーバーレイを実行", L"권장: 가능하면 내장/절전 GPU로 오버레이 실행", L"Совет: по возможности запускайте оверлей на встроенном/энергосберегающем GPU"},
    {L"高性能 GPU 可能与游戏争用调度资源，造成卡顿", L"A high-performance GPU may compete with the game and cause stutter", L"高性能 GPU はゲームと処理時間を奪い合い、カクつく場合があります", L"고성능 GPU는 게임과 리소스를 경쟁해 끊김이 발생할 수 있습니다", L"Высокопроизводительный GPU может конкурировать с игрой и вызывать рывки"},
    {L"从外部启动 start_without_steam.bat", L"Launch with start_without_steam.bat", L"start_without_steam.bat で起動", L"start_without_steam.bat로 실행", L"Запустить через start_without_steam.bat"},
    {L"VTube Studio 已关闭，请选择重启方式", L"VTube Studio has closed. Choose how to restart it", L"VTube Studio を終了しました。再起動方法を選択してください", L"VTube Studio가 종료되었습니다. 다시 시작할 방법을 선택하세요", L"VTube Studio закрыта. Выберите способ перезапуска"},
    {L"Steam 选项会直接打开 VTube Studio.exe；外部选项会运行安装目录中的 start_without_steam.bat。", L"The Steam option opens VTube Studio.exe directly. The external option runs start_without_steam.bat from the installation folder.", L"Steam は VTube Studio.exe を直接開きます。外部起動はインストール先の start_without_steam.bat を実行します。", L"Steam 옵션은 VTube Studio.exe를 직접 실행합니다. 외부 실행은 설치 폴더의 start_without_steam.bat를 실행합니다.", L"Вариант Steam напрямую открывает VTube Studio.exe. Внешний запуск выполняет start_without_steam.bat из папки установки."},
    {L"Windows 只能一键指定“节能”或“高性能”GPU，无法可靠指定这张额外显卡。\n\n本次没有修改设置。", L"Windows can select only a power-saving or high-performance GPU automatically; this additional adapter cannot be selected reliably.\n\nNo settings were changed.", L"Windows が自動指定できるのは「省電力」または「高パフォーマンス」GPU のみで、この追加 GPU は確実に指定できません。\n\n設定は変更されませんでした。", L"Windows에서는 절전 또는 고성능 GPU만 자동 지정할 수 있어 이 추가 GPU를 안정적으로 지정할 수 없습니다.\n\n설정은 변경되지 않았습니다.", L"Windows позволяет автоматически выбрать только энергосберегающий или высокопроизводительный GPU; надёжно указать этот дополнительный адаптер нельзя.\n\nНастройки не изменены."},
    {L"没有找到 VTube Studio 安装目录。\n\n程序已经检查正在运行的 VTS、Steam 库和默认安装目录。请先启动一次 VTube Studio 后再尝试。", L"The VTube Studio installation folder was not found.\n\nRunning VTS instances, Steam libraries, and default locations were checked. Launch VTube Studio once, then try again.", L"VTube Studio のインストール先が見つかりません。\n\n実行中の VTS、Steam ライブラリ、既定の場所を確認しました。一度 VTube Studio を起動してから再試行してください。", L"VTube Studio 설치 폴더를 찾을 수 없습니다.\n\n실행 중인 VTS, Steam 라이브러리 및 기본 경로를 확인했습니다. VTube Studio를 한 번 실행한 뒤 다시 시도하세요.", L"Папка установки VTube Studio не найдена.\n\nПроверены запущенные экземпляры VTS, библиотеки Steam и стандартные папки. Один раз запустите VTube Studio и повторите попытку."},
    {L"将把 VTube Studio 切换到：\n\n", L"VTube Studio will be switched to:\n\n", L"VTube Studio の使用 GPU を次に切り替えます：\n\n", L"VTube Studio를 다음 GPU로 전환합니다:\n\n", L"VTube Studio будет переключена на:\n\n"},
    {L"\n\n这会暂时中断面捕，并依次执行：\n1. 正常关闭 VTube Studio\n2. 保存 Windows 显卡偏好\n3. 运行 VTube Studio\n4. 自动重启覆盖层\n\n是否继续？", L"\n\nTracking will be interrupted briefly while the app:\n1. Closes VTube Studio normally\n2. Saves the Windows graphics preference\n3. Starts VTube Studio\n4. Restarts the overlay automatically\n\nContinue?", L"\n\nトラッキングを一時中断し、次の処理を行います：\n1. VTube Studio を正常終了\n2. Windows の GPU 設定を保存\n3. VTube Studio を起動\n4. オーバーレイを自動再起動\n\n続行しますか？", L"\n\n잠시 트래킹을 중단하고 다음 작업을 수행합니다:\n1. VTube Studio 정상 종료\n2. Windows 그래픽 기본 설정 저장\n3. VTube Studio 실행\n4. 오버레이 자동 재시작\n\n계속할까요?", L"\n\nОтслеживание ненадолго прервётся. Программа:\n1. Корректно закроет VTube Studio\n2. Сохранит графические настройки Windows\n3. Запустит VTube Studio\n4. Автоматически перезапустит оверлей\n\nПродолжить?"},
    {L"写入 Windows 显卡偏好失败，本次没有关闭 VTube Studio。", L"Could not save the Windows graphics preference. VTube Studio was not closed.", L"Windows の GPU 設定を保存できませんでした。VTube Studio は終了していません。", L"Windows 그래픽 기본 설정을 저장하지 못했습니다. VTube Studio는 종료하지 않았습니다.", L"Не удалось сохранить графические настройки Windows. VTube Studio не была закрыта."},
    {L"无法向 VTube Studio 发送正常退出命令。为了保护当前状态，程序不会强制结束它。", L"VTube Studio could not be asked to close normally. To protect its current state, it will not be terminated forcibly.", L"VTube Studio に正常終了を要求できませんでした。現在の状態を保護するため、強制終了は行いません。", L"VTube Studio에 정상 종료 요청을 보내지 못했습니다. 현재 상태를 보호하기 위해 강제 종료하지 않습니다.", L"Не удалось запросить корректное завершение VTube Studio. Чтобы сохранить текущее состояние, принудительное завершение не выполняется."},
    {L"VTube Studio 在 15 秒内没有退出。为了避免丢失状态，程序没有强制关闭它，GPU 设置也已还原。", L"VTube Studio did not close within 15 seconds. It was not terminated forcibly, and the GPU setting was restored to avoid losing state.", L"VTube Studio が15秒以内に終了しませんでした。状態を失わないよう強制終了せず、GPU 設定を元に戻しました。", L"VTube Studio가 15초 안에 종료되지 않았습니다. 상태 손실을 방지하기 위해 강제 종료하지 않고 GPU 설정을 복원했습니다.", L"VTube Studio не закрылась за 15 секунд. Чтобы не потерять состояние, принудительное завершение не выполнялось, а настройка GPU восстановлена."},
    {L"未能运行 start_without_steam.bat。显卡偏好已还原，请手动启动 VTube Studio。", L"Could not run start_without_steam.bat. The graphics preference was restored; start VTube Studio manually.", L"start_without_steam.bat を実行できませんでした。GPU 設定を元に戻しました。VTube Studio を手動で起動してください。", L"start_without_steam.bat를 실행하지 못했습니다. 그래픽 기본 설정을 복원했으니 VTube Studio를 직접 실행하세요.", L"Не удалось запустить start_without_steam.bat. Графические настройки восстановлены; запустите VTube Studio вручную."},
    {L"VTube Studio 已开始重启，但覆盖层自动重启助手启动失败。\n\n请稍后手动运行 start_overlay.cmd。", L"VTube Studio is restarting, but the overlay restart helper could not be started.\n\nRun start_overlay.cmd manually after VTS starts.", L"VTube Studio の再起動は始まりましたが、オーバーレイ再起動ヘルパーを起動できませんでした。\n\nVTS 起動後に start_overlay.cmd を手動で実行してください。", L"VTube Studio가 다시 시작 중이지만 오버레이 재시작 도우미를 실행하지 못했습니다.\n\nVTS가 시작된 뒤 start_overlay.cmd를 직접 실행하세요.", L"VTube Studio перезапускается, но не удалось запустить помощник перезапуска оверлея.\n\nПосле запуска VTS выполните start_overlay.cmd вручную."},
    {L"  接收 ", L"  Receive ", L"  受信 ", L"  수신 ", L"  Приём "},
    {L"  缩放 ", L"  Scale ", L"  拡縮 ", L"  크기 조정 ", L"  Масштаб "},
    {L"  上屏 ", L"  Present ", L"  表示 ", L"  표시 ", L"  Вывод "},
    {L"渲染 ", L"Render ", L"描画 ", L"렌더링 ", L"Рендер "},
    {L"  对齐 ", L"  Match ", L"  一致率 ", L"  일치율 ", L"  Совпадение "},
    {L"  面数 ", L"  ArtMeshes ", L"  ArtMesh数 ", L"  ArtMesh 수 ", L"  ArtMesh "},
    {L"  道具 ", L"  Items ", L"  アイテム ", L"  아이템 ", L"  Предметы "},
    {L"  显存 ", L"  VRAM ", L"  VRAM ", L"  VRAM ", L"  VRAM "},
    {L"  内存 ", L"  RAM ", L"  RAM ", L"  RAM ", L"  RAM "},
    {L"  目标 ", L"  Target ", L"  目標 ", L"  목표 ", L"  Цель "},
    {L"VTS配置 ", L"VTS setting ", L"VTS設定 ", L"VTS 설정 ", L"Настройка VTS "},
    {L"  VTS实时 ", L"  VTS live ", L"  VTS実測 ", L"  VTS 실시간 ", L"  VTS сейчас "},
    {L"接收 ", L"Receive ", L"受信 ", L"수신 ", L"Приём "},
    {L"显存 ", L"VRAM ", L"VRAM ", L"VRAM ", L"VRAM "},
    {L" MB  内存 ", L" MB  RAM ", L" MB  RAM ", L" MB  RAM ", L" МБ  RAM "},
    {L"VTS运行 ", L"VTS uptime ", L"VTS稼働時間 ", L"VTS 실행 시간 ", L"VTS работает "},
    {L"  模型 ", L"  Model ", L"  モデル ", L"  모델 ", L"  Модель "},
    {L"  对齐率 ", L"  Match ", L"  一致率 ", L"  일치율 ", L"  Совпадение "},
    {L"完美（推荐对齐率区间）", L"Ideal (recommended match range)", L"最適（推奨一致率）", L"최적(권장 일치 범위)", L"Идеально (рекомендуемый диапазон)"},
    {L"绝顶质量", L"Maximum quality", L"最高品質", L"최고 품질", L"Максимальное качество"},
    {L"极高质量", L"Very high quality", L"非常に高品質", L"매우 높은 품질", L"Очень высокое качество"},
    {L"超高质量", L"High quality", L"高品質", L"높은 품질", L"Высокое качество"},
    {L"较高质量", L"Balanced toward quality", L"品質優先", L"품질 우선", L"Упор на качество"},
    {L"质量", L"Quality", L"品質", L"품질", L"Качество"},
    {L"究极性能", L"Maximum performance", L"最高パフォーマンス", L"최고 성능", L"Максимальная производительность"},
    {L"极高性能", L"Very high performance", L"非常に高パフォーマンス", L"매우 높은 성능", L"Очень высокая производительность"},
    {L"超高性能", L"High performance", L"高パフォーマンス", L"높은 성능", L"Высокая производительность"},
    {L"较高性能", L"Balanced toward performance", L"パフォーマンス優先", L"성능 우선", L"Упор на производительность"},
    {L"性能", L"Performance", L"パフォーマンス", L"성능", L"Производительность"},
    {L"当前性能评级：", L"Current rating: ", L"現在の評価：", L"현재 등급: ", L"Текущая оценка: "},
    {L"渲染 --  VTS --  对齐率 --", L"Render --  VTS --  Match --", L"描画 --  VTS --  一致率 --", L"렌더링 --  VTS --  일치율 --", L"Рендер --  VTS --  Совпадение --"},
    {L"当前性能评级：--", L"Current rating: --", L"現在の評価：--", L"현재 등급: --", L"Текущая оценка: --"},
    {L"  [Spout 当前]", L"  [Current Spout]", L"  [現在の Spout]", L"  [현재 Spout]", L"  [Текущий Spout]"},
    {L"自定义帧率", L"Custom frame rate", L"カスタムフレームレート", L"사용자 지정 프레임 속도", L"Своя частота кадров"},
    {L"请输入 1–240 FPS：", L"Enter 1–240 FPS:", L"1～240 FPS を入力：", L"1~240 FPS 입력:", L"Введите 1–240 FPS:"},
    {L"输入内容会在这里直接显示，确定后才会应用。", L"Applied after you select OK.", L"「OK」を選択すると適用されます。", L"확인을 선택하면 적용됩니다.", L"Применяется после нажатия «ОК»."},
    {L"确定", L"OK", L"OK", L"확인", L"ОК"},
    {L"编辑自定义语言配置…", L"Edit custom language file…", L"カスタム言語ファイルを編集…", L"사용자 언어 파일 편집…", L"Изменить файл своего языка…"},
    {L"无法打开自定义语言配置文件：\n", L"Could not open the custom language file:\n", L"カスタム言語ファイルを開けませんでした：\n", L"사용자 언어 파일을 열 수 없습니다:\n", L"Не удалось открыть файл своего языка:\n"},
    {L"自定义语言", L"Custom language", L"カスタム言語", L"사용자 언어", L"Свой язык"},
    {L"采集", L"Capture", L"収集中", L"수집", L"Сбор"},
    {L"等待 VTS 分辨率", L"Waiting for VTS resolution", L"VTS 解像度を待機中", L"VTS 해상도 대기 중", L"Ожидание разрешения VTS"},
    {L"预热中", L"Warming up", L"ウォームアップ中", L"준비 중", L"Прогрев"},
    {L"采集组", L"Capture group", L"収集グループ", L"수집 그룹", L"Группа сбора"},
    {L"请在 VTube Studio 中手动设置输出分辨率：", L"Manually set the VTube Studio output resolution:", L"VTube Studio の出力解像度を手動で設定してください：", L"VTube Studio 출력 해상도를 직접 설정하세요:", L"Вручную задайте разрешение вывода VTube Studio:"},
    {L"程序缩放分辨率将自动设置为：", L"The app render resolution will be set automatically to:", L"アプリの描画解像度は自動的に次へ設定されます：", L"앱 렌더링 해상도는 자동으로 다음 값으로 설정됩니다:", L"Разрешение рендеринга программы будет автоматически задано:"},
    {L"取消将结束采集并保留已经写入的原始数据。", L"Cancel ends capture and keeps the raw data already written.", L"キャンセルすると収集を終了し、書き込み済みの生データを保持します。", L"취소하면 수집을 종료하고 이미 기록한 원시 데이터를 보존합니다.", L"Отмена завершит сбор и сохранит уже записанные исходные данные."},
    {L"分辨率性能采集", L"Resolution performance capture", L"解像度パフォーマンス収集", L"해상도 성능 수집", L"Сбор данных производительности разрешений"},
    {L"采集已完成。", L"Capture completed.", L"収集が完了しました。", L"수집이 완료되었습니다.", L"Сбор завершён."},
    {L"采集已停止，已保留现有数据。", L"Capture stopped. Existing data was kept.", L"収集を停止しました。既存データは保持されています。", L"수집을 중지했습니다. 기존 데이터는 보존되었습니다.", L"Сбор остановлен. Имеющиеся данные сохранены."},
    {L"采集数据已保存到：", L"Capture data was saved to:", L"収集データの保存先：", L"수집 데이터 저장 위치:", L"Данные сбора сохранены в:"},
    {L"尚未接收到 VTube Studio 的 Spout2 画面。", L"No VTube Studio Spout2 frame has been received yet.", L"VTube Studio の Spout2 映像をまだ受信していません。", L"아직 VTube Studio Spout2 화면을 수신하지 못했습니다.", L"Кадр Spout2 от VTube Studio ещё не получен."},
    {L"请先连接 VTS API，以采集 VTS 实时帧数。", L"Connect the VTS API first to collect live VTS FPS.", L"VTS の実測 FPS を収集するには、先に VTS API へ接続してください。", L"VTS 실시간 FPS를 수집하려면 먼저 VTS API를 연결하세요.", L"Сначала подключите VTS API, чтобы собирать текущий FPS VTS."},
    {L"无法创建采集文件。", L"Could not create capture files.", L"収集ファイルを作成できませんでした。", L"수집 파일을 만들 수 없습니다.", L"Не удалось создать файлы сбора."},
    {L"停止并保存当前采集", L"Stop and save capture", L"収集を停止して保存", L"수집 중지 및 저장", L"Остановить и сохранить сбор"},
    {L"分辨率基准采集", L"Resolution benchmark", L"解像度ベンチマーク", L"해상도 벤치마크", L"Тест разрешений"},
    {L"你确定要进行该功能的使用吗？", L"Are you sure you want to use this feature?", L"この機能を使用しますか？", L"이 기능을 사용하시겠습니까?", L"Вы уверены, что хотите использовать эту функцию?"},
    {L"该功能设计在多组分辨率测试时获取性能数据，一般你是用不到该功能的！\n\n是否继续？", L"This feature collects performance data across multiple resolution combinations. Most users do not need it.\n\nContinue?", L"この機能は、複数の解像度の組み合わせでパフォーマンスデータを収集するためのものです。通常は使用する必要はありません。\n\n続行しますか？", L"이 기능은 여러 해상도 조합의 성능 데이터를 수집하기 위한 기능입니다. 일반적인 사용에는 필요하지 않습니다.\n\n계속하시겠습니까?", L"Эта функция собирает данные производительности для нескольких сочетаний разрешений. Большинству пользователей она не требуется.\n\nПродолжить?"},
    {L"继续", L"Continue", L"続行", L"계속", L"Продолжить"},
    {L"组", L"groups", L"グループ", L"그룹", L"групп"},
    {L"条", L"samples", L"件", L"개", L"замеров"},
    {L"设置完成后点击“确定”。程序检测到分辨率匹配后会先预热，再开始采集。", L"After setting it, select OK. Once both resolutions match, the app warms up and starts capture.", L"設定後に「OK」を選択してください。解像度の一致を検出すると、ウォームアップ後に収集を開始します。", L"설정 후 확인을 선택하세요. 해상도가 일치하면 준비 후 수집을 시작합니다.", L"После настройки нажмите «ОК». Когда разрешения совпадут, программа выполнит прогрев и начнёт сбор."},
    {L"打开采集目录", L"Open capture folder", L"収集フォルダーを開く", L"수집 폴더 열기", L"Открыть папку сбора"},
    {L"开始分辨率基准采集", L"Start resolution benchmark", L"解像度ベンチマークを開始", L"해상도 벤치마크 시작", L"Начать тест разрешений"},
    {L"将测试 7×7 共 49 组分辨率组合，每组采集 24 条数据。", L"Tests all 49 combinations in a 7×7 resolution matrix and collects 24 samples per group.", L"7×7 の全49解像度組み合わせをテストし、各グループで24件を収集します。", L"7×7 해상도 조합 49개를 모두 테스트하고 그룹마다 샘플 24개를 수집합니다.", L"Будут проверены все 49 сочетаний разрешений 7×7, по 24 замера на группу."},
    {L"每张 GPU 预计用时约", L"Estimated time per GPU:", L"GPU 1基あたりの予定時間：", L"GPU당 예상 시간:", L"Примерное время на один GPU:"},
    {L"分钟", L"minutes", L"分", L"분", L"мин"},
    {L"分钟。", L"minutes.", L"分。", L"분입니다.", L"мин."},
    {L"测试分辨率：", L"Test resolutions:", L"テスト解像度：", L"테스트 해상도:", L"Тестируемые разрешения:"},
    {L"640×480、960×540、1280×720、1600×900、1920×1080、2560×1440、3840×2160", L"640×480, 960×540, 1280×720, 1600×900, 1920×1080, 2560×1440, 3840×2160", L"640×480、960×540、1280×720、1600×900、1920×1080、2560×1440、3840×2160", L"640×480, 960×540, 1280×720, 1600×900, 1920×1080, 2560×1440, 3840×2160", L"640×480, 960×540, 1280×720, 1600×900, 1920×1080, 2560×1440, 3840×2160"},
    {L"采集期间请勿运行游戏、渲染或其他高负载任务，否则结果会失真。", L"Do not run games, rendering, or other heavy workloads during capture because they will distort the results.", L"収集中はゲーム、レンダリング、その他の高負荷処理を実行しないでください。結果が不正確になります。", L"수집 중에는 게임, 렌더링 또는 기타 고부하 작업을 실행하지 마세요. 결과가 왜곡될 수 있습니다.", L"Во время сбора не запускайте игры, рендеринг и другие тяжёлые задачи: они исказят результат."},
    {L"请保持 VTube Studio、Spout2 与插件 API 正常运行。", L"Keep VTube Studio, Spout2, and the plugin API running.", L"VTube Studio、Spout2、プラグイン API を実行したままにしてください。", L"VTube Studio, Spout2 및 플러그인 API를 계속 실행해 주세요.", L"Не закрывайте VTube Studio, Spout2 и API плагинов."},
    {L"程序会自动调整分辨率，完成或停止后恢复窗口。", L"The app adjusts resolutions automatically and restores the windows after completion or cancellation.", L"解像度は自動調整され、完了または停止後にウィンドウが復元されます。", L"프로그램이 해상도를 자동 조정하고 완료 또는 중지 후 창을 복원합니다.", L"Программа автоматически меняет разрешения и восстанавливает окна после завершения или остановки."},
    {L"测试当前 GPU", L"Test current GPU", L"現在の GPU をテスト", L"현재 GPU 테스트", L"Тестировать текущий GPU"},
    {L"测试全部 GPU", L"Test all GPUs", L"すべての GPU をテスト", L"모든 GPU 테스트", L"Тестировать все GPU"},
    {L"预计约", L"about", L"約", L"약", L"около"},
    {L"无法自动切换到下一张 GPU，采集已停止并保留现有 CSV。", L"Could not switch automatically to the next GPU. Capture stopped and the existing CSV was kept.", L"次の GPU に自動切り替えできませんでした。収集を停止し、既存の CSV は保持しました。", L"다음 GPU로 자동 전환하지 못했습니다. 수집을 중지하고 기존 CSV를 보존했습니다.", L"Не удалось автоматически переключиться на следующий GPU. Сбор остановлен, существующий CSV сохранён."},
    {L"写入下一张 GPU 的 Windows 显卡偏好失败，采集已停止并保留现有 CSV。", L"Could not save the Windows graphics preference for the next GPU. Capture stopped and the existing CSV was kept.", L"次の GPU の Windows グラフィックス設定を保存できませんでした。収集を停止し、既存の CSV は保持しました。", L"다음 GPU의 Windows 그래픽 기본 설정을 저장하지 못했습니다. 수집을 중지하고 기존 CSV를 보존했습니다.", L"Не удалось сохранить настройку графики Windows для следующего GPU. Сбор остановлен, существующий CSV сохранён."},
    {L"无法正常关闭 VTube Studio，采集已停止并保留现有 CSV。", L"Could not close VTube Studio normally. Capture stopped and the existing CSV was kept.", L"VTube Studio を正常に終了できませんでした。収集を停止し、既存の CSV は保持しました。", L"VTube Studio를 정상적으로 종료하지 못했습니다. 수집을 중지하고 기존 CSV를 보존했습니다.", L"Не удалось корректно закрыть VTube Studio. Сбор остановлен, существующий CSV сохранён."},
    {L"VTube Studio 未能在 15 秒内关闭，采集已停止并保留现有 CSV。", L"VTube Studio did not close within 15 seconds. Capture stopped and the existing CSV was kept.", L"VTube Studio が15秒以内に終了しませんでした。収集を停止し、既存の CSV は保持しました。", L"VTube Studio가 15초 안에 종료되지 않았습니다. 수집을 중지하고 기존 CSV를 보존했습니다.", L"VTube Studio не закрылся за 15 секунд. Сбор остановлен, существующий CSV сохранён."},
    {L"无法启动 VTube Studio，采集已停止并保留现有 CSV。", L"Could not start VTube Studio. Capture stopped and the existing CSV was kept.", L"VTube Studio を起動できませんでした。収集を停止し、既存の CSV は保持しました。", L"VTube Studio를 시작하지 못했습니다. 수집을 중지하고 기존 CSV를 보존했습니다.", L"Не удалось запустить VTube Studio. Сбор остановлен, существующий CSV сохранён."},
    {L"覆盖层自动重启失败，采集已停止并保留现有 CSV。", L"The overlay could not restart automatically. Capture stopped and the existing CSV was kept.", L"オーバーレイを自動再起動できませんでした。収集を停止し、既存の CSV は保持しました。", L"오버레이를 자동으로 다시 시작하지 못했습니다. 수집을 중지하고 기존 CSV를 보존했습니다.", L"Не удалось автоматически перезапустить оверлей. Сбор остановлен, существующий CSV сохранён."},
};

std::atomic<UiLanguage> gLanguage{UiLanguage::SimplifiedChinese};
std::once_flag gTraditionalTranslationsOnce;
std::array<std::wstring, ARRAYSIZE(kTranslations)> gTraditionalTranslations;
std::mutex gCustomLanguageMutex;
std::array<std::wstring, ARRAYSIZE(kTranslations)> gCustomTranslations;
std::wstring gCustomLanguageName = L"Custom (INI)";
std::wstring gCustomFontFace = L"Segoe UI";
std::filesystem::path gCustomLanguagePath;

void ReplaceAll(
    std::wstring& text, std::wstring_view from, std::wstring_view to) {
    if (from.empty()) return;
    size_t position = 0;
    while ((position = text.find(from, position)) != std::wstring::npos) {
        text.replace(position, from.size(), to);
        position += to.size();
    }
}

std::wstring ToTraditionalChinese(const wchar_t* simplified) {
    if (!simplified || !*simplified) return simplified ? simplified : L"";
    const int required = LCMapStringEx(
        L"zh-Hant", LCMAP_TRADITIONAL_CHINESE, simplified, -1,
        nullptr, 0, nullptr, nullptr, 0);
    if (required <= 1) return simplified;
    std::wstring result(static_cast<size_t>(required), L'\0');
    if (!LCMapStringEx(
            L"zh-Hant", LCMAP_TRADITIONAL_CHINESE, simplified, -1,
            result.data(), required, nullptr, nullptr, 0)) {
        return simplified;
    }
    result.resize(static_cast<size_t>(required - 1));

    // Windows performs the character conversion. These replacements adapt
    // common UI terms to wording familiar to Traditional Chinese users.
    constexpr std::pair<std::wstring_view, std::wstring_view> terminology[] = {
        {std::wstring_view(L"文件夾"), std::wstring_view(L"資料夾")},
        {std::wstring_view(L"文件"), std::wstring_view(L"檔案")},
        {std::wstring_view(L"設置"), std::wstring_view(L"設定")},
        {std::wstring_view(L"默認"), std::wstring_view(L"預設")},
        {std::wstring_view(L"程序"), std::wstring_view(L"程式")},
        {std::wstring_view(L"鼠標"), std::wstring_view(L"滑鼠")},
        {std::wstring_view(L"全屏"), std::wstring_view(L"全螢幕")},
        {std::wstring_view(L"屏幕"), std::wstring_view(L"螢幕")},
        {std::wstring_view(L"信息"), std::wstring_view(L"資訊")},
        {std::wstring_view(L"緩存"), std::wstring_view(L"快取")},
        {std::wstring_view(L"視頻"), std::wstring_view(L"影片")},
        {std::wstring_view(L"界面"), std::wstring_view(L"介面")},
    };
    for (const auto& [from, to] : terminology) ReplaceAll(result, from, to);
    return result;
}

void InitializeTraditionalTranslations() {
    for (size_t index = 0; index < ARRAYSIZE(kTranslations); ++index) {
        gTraditionalTranslations[index] =
            ToTraditionalChinese(kTranslations[index].zh);
    }
}

std::wstring TranslationKey(const wchar_t* source) {
    // Stable FNV-1a key based on the internal source identity. English stays
    // the editable reference value, while repeated English labels can still
    // be translated differently and later wording changes do not rename keys.
    std::uint64_t hash = 14695981039346656037ull;
    for (const wchar_t* cursor = source; cursor && *cursor; ++cursor) {
        const std::uint16_t codeUnit = static_cast<std::uint16_t>(*cursor);
        hash ^= static_cast<std::uint8_t>(codeUnit & 0xffu);
        hash *= 1099511628211ull;
        hash ^= static_cast<std::uint8_t>((codeUnit >> 8u) & 0xffu);
        hash *= 1099511628211ull;
    }
    wchar_t key[20]{};
    swprintf_s(key, L"T_%016llX", static_cast<unsigned long long>(hash));
    return key;
}

std::wstring EscapeIniValue(const wchar_t* value) {
    const std::wstring text = value ? value : L"";
    size_t firstNonSpace = 0;
    while (firstNonSpace < text.size() && text[firstNonSpace] == L' ') {
        ++firstNonSpace;
    }
    size_t lastNonSpace = text.size();
    while (lastNonSpace > firstNonSpace && text[lastNonSpace - 1] == L' ') {
        --lastNonSpace;
    }
    std::wstring escaped;
    escaped.reserve(text.size() + 16);
    for (size_t index = 0; index < text.size(); ++index) {
        const wchar_t character = text[index];
        if (character == L'\\') escaped += L"\\\\";
        else if (character == L'\n') escaped += L"\\n";
        else if (character == L'\r') escaped += L"\\r";
        else if (character == L'\t') escaped += L"\\t";
        else if (character == L' ' &&
                 (index < firstNonSpace || index >= lastNonSpace)) {
            escaped += L"\\s";
        } else escaped.push_back(character);
    }
    return escaped;
}

std::wstring UnescapeIniValue(const std::wstring& value) {
    std::wstring result;
    result.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] != L'\\' || index + 1 >= value.size()) {
            result.push_back(value[index]);
            continue;
        }
        const wchar_t escaped = value[++index];
        if (escaped == L'n') result.push_back(L'\n');
        else if (escaped == L'r') result.push_back(L'\r');
        else if (escaped == L't') result.push_back(L'\t');
        else if (escaped == L's') result.push_back(L' ');
        else result.push_back(escaped);
    }
    return result;
}

std::filesystem::path ExecutableDirectory() {
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
}

std::string WideToUtf8(std::wstring_view value) {
    if (value.empty()) return {};
    const int bytes = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return {};
    std::string result(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), bytes, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(std::string_view value) {
    if (value.empty()) return {};
    const int characters = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (characters <= 0) return {};
    std::wstring result(static_cast<size_t>(characters), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), characters);
    return result;
}

bool WriteCustomLanguageTemplate(const std::filesystem::path& path) {
    std::wstring content =
        L"; VTSFloat_Meow custom language template\r\n"
        L"; English is the reference language and the default fallback.\r\n"
        L"; Keep every T_ key unchanged and edit the text between '=' and ' #'.\r\n"
        L"; Text after ' #' is an English note and is not displayed.\r\n"
        L"; Write \\# if the translated text itself needs a literal # after a space.\r\n"
        L"; Use \\n for a line break, \\s for an edge space, and \\\\ for a backslash.\r\n"
        L"; Re-select Custom (INI) in the Language menu to reload this file.\r\n"
        L"\r\n[language]\r\n"
        L"name=Custom (INI)\r\n"
        L"font=Segoe UI\r\n"
        L"base=en\r\n"
        L"\r\n[translations]\r\n";
    for (const auto& entry : kTranslations) {
        content += TranslationKey(entry.zh);
        content += L"=";
        content += EscapeIniValue(entry.en);
        content += L" #";
        content += EscapeIniValue(entry.en);
        content += L"\r\n";
    }

    const HANDLE file = CreateFileW(
        path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_EXISTS;
    }
    constexpr std::uint8_t bom[] = { 0xef, 0xbb, 0xbf };
    DWORD written = 0;
    bool ok = WriteFile(file, &bom, sizeof(bom), &written, nullptr) &&
        written == sizeof(bom);
    const std::string utf8 = WideToUtf8(content);
    if (ok && !utf8.empty()) {
        const DWORD bytes = static_cast<DWORD>(utf8.size());
        ok = WriteFile(file, utf8.data(), bytes, &written, nullptr) &&
            written == bytes;
    }
    CloseHandle(file);
    if (!ok) {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
    return ok;
}

bool ReadCustomLanguageIni(
    const std::filesystem::path& path,
    std::unordered_map<std::wstring, std::wstring>& language,
    std::unordered_map<std::wstring, std::wstring>& translations) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::string bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 &&
        static_cast<std::uint8_t>(bytes[0]) == 0xef &&
        static_cast<std::uint8_t>(bytes[1]) == 0xbb &&
        static_cast<std::uint8_t>(bytes[2]) == 0xbf) {
        bytes.erase(0, 3);
    }

    std::istringstream lines(bytes);
    std::string line;
    std::string section;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == ';' || line[first] == '#') {
            continue;
        }
        if (line[first] == '[') {
            const size_t close = line.find(']', first + 1);
            if (close != std::string::npos) {
                section = line.substr(first + 1, close - first - 1);
            }
            continue;
        }
        const size_t separator = line.find('=', first);
        if (separator == std::string::npos) continue;
        size_t keyEnd = separator;
        while (keyEnd > first &&
               (line[keyEnd - 1] == ' ' || line[keyEnd - 1] == '\t')) {
            --keyEnd;
        }
        const std::wstring key = Utf8ToWide(
            std::string_view(line).substr(first, keyEnd - first));
        std::string_view rawValue(line);
        rawValue.remove_prefix(separator + 1);
        // A space followed by # starts the optional English reference note.
        // Requiring the space keeps ordinary hashtags valid; " \\#" can be
        // used when the translated text itself needs that exact sequence.
        if (section == "translations") {
            const size_t comment = rawValue.find(" #");
            if (comment != std::string_view::npos) {
                rawValue = rawValue.substr(0, comment);
            }
        }
        const std::wstring value = Utf8ToWide(rawValue);
        if (key.empty()) continue;
        if (section == "language") language[key] = value;
        else if (section == "translations") translations[key] = value;
    }
    return true;
}

void AppendMissingCustomTranslations(
    const std::filesystem::path& path,
    const std::vector<size_t>& missing) {
    if (missing.empty()) return;
    std::wstring content = L"\r\n[translations]\r\n";
    for (const size_t index : missing) {
        content += TranslationKey(kTranslations[index].zh);
        content += L"=";
        content += EscapeIniValue(kTranslations[index].en);
        content += L" #";
        content += EscapeIniValue(kTranslations[index].en);
        content += L"\r\n";
    }
    std::ofstream output(path, std::ios::binary | std::ios::app);
    const std::string utf8 = WideToUtf8(content);
    output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
}

std::filesystem::path ResolveCustomLanguagePath() {
    if (!gCustomLanguagePath.empty()) return gCustomLanguagePath;

    // Tests can use an isolated template without touching the user's actual
    // language file.
    std::array<wchar_t, 32768> overridePath{};
    const DWORD overrideLength = GetEnvironmentVariableW(
        L"VTSFLOAT_CUSTOM_LANGUAGE_PATH", overridePath.data(),
        static_cast<DWORD>(overridePath.size()));
    if (overrideLength > 0 && overrideLength < overridePath.size()) {
        const std::filesystem::path path(
            std::wstring(overridePath.data(), overrideLength));
        std::error_code error;
        if (std::filesystem::exists(path, error) ||
            WriteCustomLanguageTemplate(path)) {
            gCustomLanguagePath = path;
            return gCustomLanguagePath;
        }
    }

    const std::filesystem::path portable =
        ExecutableDirectory() / L"VTSFloat_Meow.custom-language.ini";
    std::array<wchar_t, 32768> localAppData{};
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", localAppData.data(),
        static_cast<DWORD>(localAppData.size()));
    const std::filesystem::path managed = length > 0 && length < localAppData.size()
        ? std::filesystem::path(std::wstring(localAppData.data(), length)) /
            L"VTSFloat_Meow.custom-language.ini"
        : std::filesystem::path{};
    std::error_code error;
    if (!managed.empty()) {
        // Copy an existing portable template once so completed translations
        // are preserved. Keep the source file untouched: it may be a sample
        // shipped with a source checkout. The managed copy is used from now on.
        if (!std::filesystem::exists(managed, error) &&
            std::filesystem::exists(portable, error)) {
            error.clear();
            std::filesystem::copy_file(
                portable, managed, std::filesystem::copy_options::none, error);
        }
        error.clear();
        if (std::filesystem::exists(managed, error) ||
            WriteCustomLanguageTemplate(managed)) {
            gCustomLanguagePath = managed;
            return gCustomLanguagePath;
        }
    }

    // LOCALAPPDATA is expected on supported Windows versions. If it is not
    // available or writable, use the temporary directory instead of leaving
    // a generated configuration file beside an EXE placed on the desktop.
    error.clear();
    const std::filesystem::path temporaryDirectory =
        std::filesystem::temp_directory_path(error);
    if (!error) {
        const std::filesystem::path fallback = temporaryDirectory /
            L"VTSFloat_Meow.custom-language.ini";
        if (std::filesystem::exists(fallback, error) ||
            WriteCustomLanguageTemplate(fallback)) {
            gCustomLanguagePath = fallback;
        }
    }
    return gCustomLanguagePath;
}

}  // namespace

UiLanguage LanguageFromLocaleName(
    const std::wstring& localeName, UiLanguage fallback) {
    if (localeName.empty()) return fallback;
    if (_wcsnicmp(localeName.c_str(), L"zh-Hant", 7) == 0 ||
        _wcsnicmp(localeName.c_str(), L"zh-TW", 5) == 0 ||
        _wcsnicmp(localeName.c_str(), L"zh-HK", 5) == 0 ||
        _wcsnicmp(localeName.c_str(), L"zh-MO", 5) == 0) {
        return UiLanguage::TraditionalChinese;
    }
    if (_wcsnicmp(localeName.c_str(), L"zh", 2) == 0) {
        return UiLanguage::SimplifiedChinese;
    }
    if (_wcsnicmp(localeName.c_str(), L"ja", 2) == 0) return UiLanguage::Japanese;
    if (_wcsnicmp(localeName.c_str(), L"ko", 2) == 0) return UiLanguage::Korean;
    if (_wcsnicmp(localeName.c_str(), L"ru", 2) == 0) return UiLanguage::Russian;
    if (_wcsnicmp(localeName.c_str(), L"en", 2) == 0) return UiLanguage::English;
    return fallback;
}

UiLanguage DetectSystemLanguage() {
    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
    const LANGID uiLanguage = GetUserDefaultUILanguage();
    if (uiLanguage != 0 && LCIDToLocaleName(
            MAKELCID(uiLanguage, SORT_DEFAULT), locale,
            ARRAYSIZE(locale), 0) > 0) {
        return LanguageFromLocaleName(locale, UiLanguage::English);
    }
    if (GetUserDefaultLocaleName(locale, ARRAYSIZE(locale))) {
        return LanguageFromLocaleName(locale, UiLanguage::English);
    }
    return UiLanguage::English;
}

UiLanguage LanguageFromCode(const std::wstring& code, UiLanguage fallback) {
    if (_wcsicmp(code.c_str(), L"zh-TW") == 0 ||
        _wcsicmp(code.c_str(), L"zh-Hant") == 0 ||
        _wcsicmp(code.c_str(), L"zh-HK") == 0 ||
        _wcsicmp(code.c_str(), L"zh-MO") == 0) {
        return UiLanguage::TraditionalChinese;
    }
    if (_wcsicmp(code.c_str(), L"zh-CN") == 0 ||
        _wcsicmp(code.c_str(), L"zh-SG") == 0 ||
        _wcsicmp(code.c_str(), L"zh") == 0) {
        return UiLanguage::SimplifiedChinese;
    }
    if (code == L"en") return UiLanguage::English;
    if (code == L"ja") return UiLanguage::Japanese;
    if (code == L"ko") return UiLanguage::Korean;
    if (code == L"ru") return UiLanguage::Russian;
    if (code == L"custom") return UiLanguage::Custom;
    return fallback;
}

const wchar_t* LanguageCode(UiLanguage language) {
    switch (language) {
    case UiLanguage::SimplifiedChinese: return L"zh-CN";
    case UiLanguage::TraditionalChinese: return L"zh-TW";
    case UiLanguage::English: return L"en";
    case UiLanguage::Japanese: return L"ja";
    case UiLanguage::Korean: return L"ko";
    case UiLanguage::Russian: return L"ru";
    case UiLanguage::Custom: return L"custom";
    }
    return L"en";
}

const wchar_t* LanguageDisplayName(UiLanguage language) {
    switch (language) {
    case UiLanguage::SimplifiedChinese: return L"简体中文";
    case UiLanguage::TraditionalChinese: return L"繁體中文";
    case UiLanguage::English: return L"English";
    case UiLanguage::Japanese: return L"日本語";
    case UiLanguage::Korean: return L"한국어";
    case UiLanguage::Russian: return L"Русский";
    case UiLanguage::Custom: {
        std::lock_guard<std::mutex> lock(gCustomLanguageMutex);
        return gCustomLanguageName.c_str();
    }
    }
    return L"English";
}

void SetLanguage(UiLanguage language) {
    if (language == UiLanguage::Custom) {
        ReloadCustomLanguage();
    }
    gLanguage.store(language, std::memory_order_relaxed);
}

UiLanguage GetLanguage() {
    return gLanguage.load(std::memory_order_relaxed);
}

bool ReloadCustomLanguage() {
    std::lock_guard<std::mutex> lock(gCustomLanguageMutex);
    for (size_t index = 0; index < ARRAYSIZE(kTranslations); ++index) {
        gCustomTranslations[index] = kTranslations[index].en;
    }
    gCustomLanguageName = L"Custom (INI)";
    gCustomFontFace = L"Segoe UI";
    const std::filesystem::path path = ResolveCustomLanguagePath();
    if (path.empty()) return false;

    std::unordered_map<std::wstring, std::wstring> language;
    std::unordered_map<std::wstring, std::wstring> translations;
    if (!ReadCustomLanguageIni(path, language, translations)) return false;
    const auto name = language.find(L"name");
    if (name != language.end() && !name->second.empty()) {
        gCustomLanguageName = UnescapeIniValue(name->second);
    }
    const auto font = language.find(L"font");
    if (font != language.end() && !font->second.empty()) {
        gCustomFontFace = UnescapeIniValue(font->second);
    }

    std::vector<size_t> missing;
    for (size_t index = 0; index < ARRAYSIZE(kTranslations); ++index) {
        const std::wstring key = TranslationKey(kTranslations[index].zh);
        const auto custom = translations.find(key);
        if (custom == translations.end()) {
            missing.push_back(index);
        } else if (!custom->second.empty()) {
            gCustomTranslations[index] = UnescapeIniValue(custom->second);
        }
        if (gCustomTranslations[index].empty()) {
            gCustomTranslations[index] = kTranslations[index].en;
        }
    }
    // Preserve user edits while extending an older template with strings
    // introduced by a newer build.
    AppendMissingCustomTranslations(path, missing);
    return true;
}

std::wstring CustomLanguageFilePath() {
    std::lock_guard<std::mutex> lock(gCustomLanguageMutex);
    return ResolveCustomLanguagePath().wstring();
}

const wchar_t* Tr(const wchar_t* simplifiedChinese) {
    const UiLanguage language = GetLanguage();
    if (!simplifiedChinese || !*simplifiedChinese ||
        language == UiLanguage::SimplifiedChinese) {
        return simplifiedChinese;
    }
    for (size_t index = 0; index < ARRAYSIZE(kTranslations); ++index) {
        const auto& entry = kTranslations[index];
        if (std::wcscmp(entry.zh, simplifiedChinese) != 0) continue;
        switch (language) {
        case UiLanguage::TraditionalChinese:
            std::call_once(
                gTraditionalTranslationsOnce, InitializeTraditionalTranslations);
            return gTraditionalTranslations[index].c_str();
        case UiLanguage::English: return entry.en;
        case UiLanguage::Japanese: return entry.ja;
        case UiLanguage::Korean: return entry.ko;
        case UiLanguage::Russian: return entry.ru;
        case UiLanguage::Custom: {
            std::lock_guard<std::mutex> lock(gCustomLanguageMutex);
            return gCustomTranslations[index].empty()
                ? entry.en : gCustomTranslations[index].c_str();
        }
        default: return entry.zh;
        }
    }
    return simplifiedChinese;
}

const wchar_t* UiFontFace() {
    switch (GetLanguage()) {
    case UiLanguage::Japanese: return L"Yu Gothic UI";
    case UiLanguage::Korean: return L"Malgun Gothic";
    case UiLanguage::SimplifiedChinese: return L"Microsoft YaHei UI";
    case UiLanguage::TraditionalChinese: return L"Microsoft JhengHei UI";
    case UiLanguage::Custom: {
        std::lock_guard<std::mutex> lock(gCustomLanguageMutex);
        return gCustomFontFace.c_str();
    }
    case UiLanguage::English:
    case UiLanguage::Russian:
    default:
        return L"Segoe UI";
    }
}

}  // namespace vtsfloat::i18n
