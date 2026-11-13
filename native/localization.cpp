#include "localization.h"
#include "cache_directory.h"

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
    {L"检查更新", L"Check for updates", L"更新を確認", L"업데이트 확인", L"Проверить обновления"},
    {L"当前版本：", L"Current version: ", L"現在のバージョン：", L"현재 버전: ", L"Текущая версия: "},
    {L"正在检查更新…", L"Checking for updates…", L"更新を確認中…", L"업데이트 확인 중…", L"Проверка обновлений…"},
    {L"尝试其他线路", L"Trying another route", L"別の経路で再試行中", L"다른 경로로 시도 중", L"Пробуем другой маршрут"},
    {L"存在新版本：", L"New version: ", L"新しいバージョン：", L"새 버전: ", L"Новая версия: "},
    {L"，是否更新？", L". Update?", L"。更新しますか？", L". 업데이트할까요?", L". Обновить?"},
    {L"暂不更新", L"Not now", L"後で", L"나중에", L"Не сейчас"},
    {L"更新并重启", L"Update and restart", L"更新して再起動", L"업데이트 후 재시작", L"Обновить и перезапустить"},
    {L"当前已是最新版本", L"You're on the latest version", L"現在のバージョンは最新です", L"현재 최신 버전입니다", L"Установлена последняя версия"},
    {L"检查失败", L"Update check failed", L"更新の確認に失敗しました", L"업데이트 확인 실패", L"Не удалось проверить обновления"},
    {L"正在下载更新…", L"Downloading update…", L"更新をダウンロード中…", L"업데이트 다운로드 중…", L"Загрузка обновления…"},
    {L"下载更新失败", L"Update download failed", L"更新のダウンロードに失敗しました", L"업데이트 다운로드 실패", L"Не удалось загрузить обновление"},
    {L"正在准备更新…", L"Preparing update…", L"更新を準備中…", L"업데이트 준비 중…", L"Подготовка обновления…"},
    {L"保存设置失败，更新已取消", L"Settings could not be saved. Update cancelled.", L"設定を保存できなかったため、更新を中止しました", L"설정을 저장할 수 없어 업데이트를 취소했습니다", L"Настройки не сохранены. Обновление отменено."},
    {L"更新成功，当前版本号为 ", L"Update complete. Current version: ", L"更新完了。現在のバージョン：", L"업데이트 완료. 현재 버전: ", L"Обновление завершено. Версия: "},
    {L"更新失败", L"Update failed", L"更新に失敗しました", L"업데이트 실패", L"Не удалось обновить"},
    {L"更新失败，已恢复旧版本", L"Update failed. Previous version restored.", L"更新に失敗したため、以前のバージョンに戻しました", L"업데이트에 실패하여 이전 버전을 복원했습니다", L"Обновление не удалось. Предыдущая версия восстановлена."},
    {L"更新未完成，已保留旧版本备份", L"Update incomplete. Previous version backup retained.", L"更新が完了していません。以前のバージョンのバックアップを保持しました", L"업데이트가 완료되지 않아 이전 버전 백업을 보관했습니다", L"Обновление не завершено. Резервная копия сохранена."},
    {L"：", L": ", L"：", L": ", L": "},
    {L"图形设置", L"Graphics", L"グラフィック", L"그래픽", L"Графика"},
    {L"选择快捷应用", L"Choose a shortcut app", L"起動するアプリを選択", L"바로 실행할 앱 선택", L"Выбрать приложение"},
    {L"无法保存快捷应用设置", L"Could not save the app shortcut.", L"アプリの設定を保存できませんでした。", L"앱 바로 가기를 저장하지 못했습니다.", L"Не удалось сохранить ярлык приложения."},
    {L"无法启动快捷应用，请检查文件路径", L"Could not launch the app. Check its file path.", L"アプリを起動できません。ファイルのパスを確認してください。", L"앱을 실행할 수 없습니다. 파일 경로를 확인하세요.", L"Не удалось запустить приложение. Проверьте путь к файлу."},
    {L"启动其他应用", L"Launch another app", L"別のアプリを起動", L"다른 앱 실행", L"Запустить приложение"},
    {L"第三方", L"Third-party", L"外部アプリ", L"외부 앱", L"Стороннее ПО"},
    {L"自动选择", L"Automatic", L"自動選択", L"자동 선택", L"Автовыбор"},
    {L"Spout2 发送源", L"Spout2 sender", L"Spout2 送信元", L"Spout2 송신 소스", L"Источник Spout2"},
    {L"请在图形设置中选择 Spout2 发送源", L"Choose a Spout2 sender in Graphics.", L"グラフィック設定で Spout2 の送信元を選択してください。", L"그래픽 설정에서 Spout2 송신 소스를 선택하세요.", L"Выберите источник Spout2 в настройках графики."},
    {L"窗口模式", L"Window mode", L"ウィンドウモード", L"창 모드", L"Режим окна"},
    {L"比例锁定", L"Lock aspect ratio", L"比率を固定", L"비율 잠금", L"Фиксация пропорций"},
    {L"桌面模式：其他程序全屏时暂停输出", L"Desktop mode: pause output while another app is fullscreen", L"デスクトップ：他のアプリが全画面の間は出力を一時停止", L"데스크톱 모드: 다른 앱이 전체 화면이면 출력 일시 정지", L"Режим рабочего стола: приостанавливать вывод, когда другое приложение на весь экран"},
    {L"裁剪", L"Crop", L"切り抜き", L"자르기", L"Обрезка"},
    {L"快捷键", L"Shortcut", L"ショートカット", L"단축키", L"Клавиши"},
    {L"请按组合键（Esc取消）", L"Press shortcut (Esc to cancel)", L"ショートカットを入力（Escで取消）", L"단축키 입력(Esc: 취소)", L"Нажмите сочетание (Esc — отмена)"},
    {L"个性化", L"Style", L"カスタム", L"꾸미기", L"Вид"},
    {L"画面编辑", L"Edit view", L"画面を編集", L"화면 편집", L"Правка кадра"},
    {L"图像", L"Image", L"画像", L"이미지", L"Изображение"},
    {L"画面混合", L"Image blending", L"画像の合成", L"이미지 혼합", L"Наложение"},
    {L"对比度", L"Contrast", L"コントラスト", L"대비", L"Контраст"},
    {L"饱和度", L"Saturation", L"彩度", L"채도", L"Насыщенность"},
    {L"色相", L"Hue", L"色相", L"색조", L"Цветовой тон"},
    {L"色温", L"Temperature", L"色温度", L"색온도", L"Темп. цвета"},
    {L"清晰度", L"Clarity", L"明瞭度", L"명료도", L"Чёткость"},
    {L"暗角", L"Vignette", L"周辺光量", L"비네팅", L"Виньетка"},
    {L"锐化", L"Sharpening", L"シャープ", L"선명 효과", L"Резкость"},
    {L"混合模式", L"Blend mode", L"描画モード", L"혼합 모드", L"Режим"},
    {L"混合强度", L"Blend strength", L"合成の強さ", L"혼합 강도", L"Сила эффекта"},
    {L"渐变网格", L"Gradient grid", L"グラデーション分割", L"그라데이션 격자", L"Сетка"},
    {L"以角色下方的平均色作为混合底色", L"Uses the average color around the model", L"モデル周囲の平均色を使用", L"모델 주변의 평균 색상 사용", L"Используется средний цвет вокруг модели"},
    {L"无法捕获屏幕背景", L"Screen backdrop is unavailable", L"画面の背景を取得できません", L"화면 배경을 캡처할 수 없음", L"Захват фона экрана недоступен"},
    {L"正常", L"Normal", L"通常", L"표준", L"Обычный"},
    {L"变暗", L"Darken", L"比較（暗）", L"어둡게 하기", L"Затемнение"},
    {L"正片叠底", L"Multiply", L"乗算", L"곱하기", L"Умножение"},
    {L"滤色", L"Screen", L"スクリーン", L"스크린", L"Экран"},
    {L"颜色减淡", L"Color Dodge", L"覆い焼きカラー", L"색상 닷지", L"Осветление основы"},
    {L"颜色加深", L"Color Burn", L"焼き込みカラー", L"색상 번", L"Затемнение основы"},
    {L"线性减淡（添加）", L"Linear Dodge (Add)", L"覆い焼き（リニア）- 加算", L"선형 닷지(추가)", L"Линейный осветлитель (добавить)"},
    {L"线性加深", L"Linear Burn", L"焼き込み（リニア）", L"선형 번", L"Линейный затемнитель"},
    {L"深色", L"Darker Color", L"カラー比較（暗）", L"어두운 색상", L"Темнее"},
    {L"实色混合", L"Hard Mix", L"ハードミックス", L"하드 혼합", L"Жёсткое смешение"},
    {L"我不想连接VTube Studio API进行使用", L"Use without VTube Studio API", L"VTube Studio API に接続せず使う", L"VTube Studio API 연결 없이 사용", L"Продолжить без VTube Studio API"},
    {L"正在连接 VTube Studio API", L"Connecting to VTube Studio API", L"VTube Studio API に接続中", L"VTube Studio API 연결 중", L"Связь с API VTube Studio"},
    {L"连接VTube Studio API", L"Connect VTube Studio API", L"VTube Studio API に接続", L"VTube Studio API 연결", L"Подключить VTube Studio API"},
    {L"可左右拖拽调整数值", L"Drag left or right to adjust", L"左右にドラッグして値を調整", L"좌우로 드래그하여 값 조절", L"Перетащите влево или вправо"},
    {L"羽化与旋转", L"Feather & Rotate", L"境界のぼかしと回転", L"가장자리 흐림 및 회전", L"Растушёвка и поворот"},
    {L"阴影", L"Shadow", L"シャドウ", L"그림자", L"Тень"},
    {L"悬停时同时淡化阴影", L"Fade shadows on hover", L"ホバー時に影も薄くする", L"호버 시 그림자도 흐리게", L"Ослаблять тени при наведении"},
    {L"启用阴影", L"Enable", L"シャドウを有効化", L"그림자 사용", L"Включить"},
    {L"内部阴影", L"Inner Shadow", L"シャドウ（内側）", L"내부 그림자", L"Внутр. тень"},
    {L"外部阴影", L"Drop Shadow", L"ドロップシャドウ", L"외부 그림자", L"Внешняя тень"},
    {L"阴影距离", L"Distance", L"距離", L"거리", L"Смещение"},
    {L"阴影扩展", L"Spread", L"スプレッド", L"확산", L"Расширение"},
    {L"阴影模糊", L"Blur", L"ぼかし", L"흐림", L"Размытие"},
    {L"阴影颜色", L"Shadow Color", L"シャドウの色", L"그림자 색상", L"Цвет тени"},
    {L"不透明度", L"Opacity", L"不透明度", L"불투명도", L"Непрозрачность"},
    {L"参考背景像素", L"Sample Background", L"背景色を取得", L"배경색 샘플링", L"Цвет из фона"},
    {L"加深程度", L"Darkening", L"暗くする量", L"어둡게 하기", L"Степень затемнения"},
    {L"颜色", L"Color", L"カラー", L"색상", L"Цвет"},
    {L"明度", L"Luminosity", L"輝度", L"광도", L"Яркость"},
    {L"旋转与翻转", L"Rotate / Flip", L"回転・反転", L"회전·뒤집기", L"Поворот / отражение"},
    {L"水平翻转", L"Flip Horiz.", L"左右反転", L"좌우 뒤집기", L"Отразить ↔"},
    {L"垂直翻转", L"Flip Vert.", L"上下反転", L"상하 뒤집기", L"Отразить ↕"},
    {L"角度", L"Angle", L"角度", L"각도", L"Угол"},
    {L"曝光", L"Exposure", L"露出", L"노출", L"Экспозиция"},
    {L"色阶", L"Levels", L"レベル", L"레벨", L"Уровни"},
    {L"黑场", L"Black", L"黒点", L"검은점", L"Чёрный"},
    {L"白场", L"White", L"白点", L"흰점", L"Белый"},
    {L"曲线", L"Curves", L"トーンカーブ", L"곡선", L"Кривые"},
    {L"原始分辨率", L"Source resolution", L"元の解像度", L"원본 해상도", L"Исходное разрешение"},
    {L"裁剪分辨率", L"Crop size", L"切り抜きサイズ", L"자르기 크기", L"Размер обрезки"},
    {L"锁定裁剪比例", L"Lock crop ratio", L"切り抜き比率を固定", L"자르기 비율 잠금", L"Зафиксировать пропорции"},
    {L"裁剪比例已锁定", L"Crop ratio locked", L"切り抜き比率を固定中", L"자르기 비율 잠김", L"Пропорции зафиксированы"},
    {L"重置", L"Reset", L"リセット", L"초기화", L"Сбросить"},
    {L"边缘羽化", L"Edge feather", L"境界のぼかし", L"가장자리 페더", L"Растушёвка краёв"},
    {L"进行画面设置时可能会增加性能开销", L"Adjusting view settings may increase processing load", L"画面設定の調整中は処理負荷が上がる場合があります", L"화면 설정을 조정하는 동안 처리 부하가 증가할 수 있습니다", L"При настройке изображения нагрузка может возрасти"},
    {L"画面编辑菜单-", L"Edit View –", L"画面編集メニュー –", L"화면 편집 메뉴 –", L"Редактирование –"},
    {L"画面设置", L"View settings", L"画面設定", L"화면 설정", L"Настройки кадра"},
    {L"重置旋转与翻转", L"Reset rotation and flip", L"回転・反転をリセット", L"회전 및 뒤집기 초기화", L"Сбросить поворот и отражение"},
    {L"按住输入框左右拖动可调整数值。", L"Drag a value left or right to adjust it.", L"数値欄を左右にドラッグして調整できます。", L"값을 좌우로 드래그하여 조정합니다.", L"Перетащите значение влево или вправо, чтобы изменить его."},
    {L"点击边缘开关羽化", L"Click an edge to toggle feathering", L"辺をクリックしてぼかしを切り替え", L"가장자리를 클릭하여 페더 켜기/끄기", L"Щёлкните по краю для растушёвки"},
    {L"左边缘", L"Left edge", L"左辺", L"왼쪽 가장자리", L"Левый край"},
    {L"上边缘", L"Top edge", L"上辺", L"위쪽 가장자리", L"Верхний край"},
    {L"右边缘", L"Right edge", L"右辺", L"오른쪽 가장자리", L"Правый край"},
    {L"下边缘", L"Bottom edge", L"下辺", L"아래쪽 가장자리", L"Нижний край"},
    {L"宽度", L"Width", L"幅", L"너비", L"Ширина"},
    {L"高度", L"Height", L"高さ", L"높이", L"Высота"},
    {L"调试", L"Debug", L"デバッグ", L"디버그", L"Отладка"},
    {L"完成", L"Done", L"完了", L"완료", L"Готово"},
    {L"点击「完成」会锁定悬浮窗口并隐藏工具栏。", L"Click Done to lock the overlay and hide the toolbar.", L"「完了」を押すと、フロートウィンドウが固定され、ツールバーが非表示になります。", L"‘완료’를 누르면 플로팅 창이 잠기고 도구 모음이 숨겨집니다.", L"Нажмите «Готово», чтобы закрепить окно и скрыть панель инструментов."},
    {L"如果担心找不到界面，你可以在系统托盘中进行重置恢复。", L"If you lose track of the window, use the system tray menu to reset its position.", L"ウィンドウが見つからないときは、システムトレイのメニューから位置をリセットできます。", L"창을 찾을 수 없으면 시스템 트레이 메뉴에서 창 위치를 초기화할 수 있습니다.", L"Если окно потерялось, сбросьте его положение через меню в системном трее."},
    {L"重新打开工具栏的快捷键：", L"Reopen the toolbar with ", L"ツールバーを再表示：", L"도구 모음 다시 열기: ", L"Показать панель снова: "},
    {L"点击「快捷键」可以自定义组合键。", L"Click Shortcut to set your own key combination.", L"「ショートカット」でキーの組み合わせを変更できます。", L"‘단축키’를 눌러 키 조합을 변경할 수 있습니다.", L"Нажмите «Клавиши», чтобы изменить сочетание."},
    {L"下一步", L"Next", L"次へ", L"다음", L"Далее"},
    {L"知道了", L"Got it", L"了解", L"확인", L"Понятно"},
    {L"解锁", L"Unlock", L"ロック解除", L"잠금 해제", L"Разблокировать"},
    {L"调试模式已打开", L"Debug mode is on", L"デバッグモード：オン", L"디버그 모드 켜짐", L"Режим отладки включён"},
    {L"打开缓存文件夹", L"Open cache folder", L"キャッシュフォルダーを開く", L"캐시 폴더 열기", L"Открыть папку кэша"},
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
    {L"如何开启 VTube Studio Plugins API", L"How to enable the VTube Studio Plugins API", L"VTube Studio Plugins API の有効化方法", L"VTube Studio Plugins API 활성화 방법", L"Как включить VTube Studio Plugins API"},
    {L"1. 打开 VTube Studio 设置。\n2. 在“常规设置”中启用“允许插件 API 访问”。\n3. 返回 VTSFloat_Meow，点击“扫描端口”。\n4. VTube Studio 弹出授权请求时，选择允许。", L"1. Open VTube Studio Settings.\n2. Under General Settings, enable “Allow Plugin API access”.\n3. Return to VTSFloat_Meow and select “Scan ports”.\n4. When VTube Studio asks for authorization, select Allow.", L"1. VTube Studio の設定を開きます。\n2. 「一般設定」で「プラグイン API へのアクセスを許可」を有効にします。\n3. VTSFloat_Meow に戻り、「ポートを検索」を選択します。\n4. VTube Studio に承認リクエストが表示されたら、許可します。", L"1. VTube Studio 설정을 엽니다.\n2. 일반 설정에서 ‘플러그인 API 접근 허용’을 켭니다.\n3. VTSFloat_Meow로 돌아가 ‘포트 검색’을 선택합니다.\n4. VTube Studio에 승인 요청이 표시되면 허용을 선택합니다.", L"1. Откройте настройки VTube Studio.\n2. В общих настройках включите «Разрешить доступ к Plugin API».\n3. Вернитесь в VTSFloat_Meow и выберите «Сканировать порты».\n4. Когда VTube Studio запросит авторизацию, нажмите «Разрешить»."},
    {L"无法打开自定义帧率输入框。", L"Could not open the custom frame-rate input.", L"カスタムフレームレート入力を開けませんでした。", L"사용자 지정 프레임 속도 입력을 열 수 없습니다.", L"Не удалось открыть ввод частоты кадров."},
    {L"请输入 1 到 240 之间的整数。", L"Enter a whole number from 1 to 240.", L"1～240 の整数を入力してください。", L"1~240 사이의 정수를 입력하세요.", L"Введите целое число от 1 до 240."},
    {L"帧率无效", L"Invalid frame rate", L"無効なフレームレート", L"잘못된 프레임 속도", L"Недопустимая частота кадров"},
    {L"普通渐变（整圈同步变色）", L"Standard gradient (whole border)", L"通常グラデーション（全周同期）", L"일반 그라데이션(전체 테두리)", L"Обычный градиент (вся рамка)"},
    {L"彩虹跑马灯", L"Rainbow chase", L"レインボーチェイス", L"무지개 추적 효과", L"Бегущая радуга"},
    {L"起始颜色", L"Start color", L"開始色", L"시작 색상", L"Начальный цвет"},
    {L"结束颜色", L"End color", L"終了色", L"끝 색상", L"Конечный цвет"},
    {L"边框样式", L"Border style", L"枠の種類", L"테두리 유형", L"Вид рамки"},
    {L"自定义", L"Solid color", L"単色", L"단색", L"Один цвет"},
    {L"跑马灯渐变", L"Chase", L"チェイス", L"추적", L"Бегущий"},
    {L"普通渐变", L"Gradient", L"グラデーション", L"그라데이션", L"Градиент"},
    {L"自定义颜色", L"Custom color", L"カスタムカラー", L"사용자 지정 색상", L"Свой цвет"},
    {L"亮度", L"Brightness", L"明るさ", L"밝기", L"Яркость"},
    {L"边框粗细", L"Border width", L"枠線の太さ", L"테두리 두께", L"Толщина"},
    {L"模型不透明度", L"Model opacity", L"モデルの不透明度", L"모델 불투명도", L"Непрозрачность"},
    {L"如果觉得界面太大或者太小，可以点击此处自行设置缩放。", L"If the interface looks too large or too small, click here to adjust its scale.", L"画面が大きすぎたり小さすぎたりする場合は、ここをクリックして表示倍率を調整できます。", L"인터페이스가 너무 크거나 작으면 여기를 눌러 크기를 조절할 수 있습니다.", L"Если интерфейс слишком большой или маленький, нажмите здесь, чтобы изменить масштаб."},
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
    {L"保存", L"Save", L"保存", L"저장", L"Сохранить"},
    {L"暂无可用表情，请确认 VTube Studio API 已连接", L"No expressions. Check the VTube Studio API connection.", L"表情がありません。VTube Studio API の接続を確認してください。", L"표정 없음. VTube Studio API 연결을 확인하세요.", L"Нет выражений. Проверьте связь с VTube Studio API."},
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
    {L"[不再提示]", L"[Don't show again]", L"[今後表示しない]", L"[다시 표시 안 함]", L"[Не показывать снова]"},
    {L"[关闭提示]", L"[Dismiss]", L"[閉じる]", L"[닫기]", L"[Закрыть]"},
    {L"当前运行在高性能显卡中，高负载场景性能将会受限", L"High-performance GPU: stutter may occur under heavy load", L"高性能 GPU：高負荷時にカクつく場合があります", L"고성능 GPU: 부하가 높으면 끊길 수 있습니다", L"Высокопроизводительный GPU: возможны рывки"},
    {L"未启用", L"Not enabled", L"未有効", L"사용 안 함", L"Не включено"},
    {L"读取失败", L"Failed", L"取得失敗", L"읽기 실패", L"Ошибка"},
    {L"VTube Studio API 未连接", L"VTube Studio API not connected", L"VTube Studio API 未接続", L"VTube Studio API 연결 안 됨", L"VTube Studio API не подключён"},
    {L"FPS / 帧时", L"FPS / frame time", L"FPS／フレーム時間", L"FPS / 프레임 시간", L"FPS / время кадра"},
    {L"已找到 VTube Studio，但当前尚未运行", L"VTube Studio was found but is not running", L"VTube Studio は見つかりましたが、起動していません", L"VTube Studio를 찾았지만 실행 중이 아닙니다", L"VTube Studio найден, но не запущен"},
    {L"或使用其他支持Spout2的任意应用", L"Or use any other app that supports Spout2.", L"Spout2 対応の他のアプリも使用できます。", L"또는 Spout2를 지원하는 다른 앱을 사용하세요.", L"Можно использовать любое другое приложение с поддержкой Spout2."},
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
    {L"请在 VTube Studio 中授权插件", L"Allow plugin in VTube Studio", L"VTube Studio でプラグインを許可", L"VTube Studio에서 플러그인 허용", L"Разрешите плагин в VTube Studio"},
    {L"你刚刚拒绝了VTSFloat_Meow", L"You just denied VTSFloat_Meow access", L"VTSFloat_Meow のアクセスを拒否しました", L"VTSFloat_Meow 접근을 거부했습니다", L"Вы отклонили доступ для VTSFloat_Meow"},
    {L"[重新发起授权]", L"[Request access again]", L"[承認を再リクエスト]", L"[승인 다시 요청]", L"[Запросить доступ снова]"},
    {L"VTube Studio API 连接已断开", L"VTube Studio API disconnected", L"VTube Studio API が切断されました", L"VTube Studio API 연결 끊김", L"VTube Studio API отключён"},
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
    {L"\n\n这会暂时中断面捕，并依次执行：\n1. 正常关闭 VTube Studio\n2. 保存 Windows 显卡偏好\n3. 运行 VTube Studio\n4. 自动重启 VTSFloat_Meow\n\n是否继续？", L"\n\nTracking will be interrupted briefly while the app:\n1. Closes VTube Studio normally\n2. Saves the Windows graphics preference\n3. Starts VTube Studio\n4. Restarts VTSFloat_Meow automatically\n\nContinue?", L"\n\nトラッキングを一時中断し、次の処理を行います：\n1. VTube Studio を正常終了\n2. Windows の GPU 設定を保存\n3. VTube Studio を起動\n4. VTSFloat_Meow を自動再起動\n\n続行しますか？", L"\n\n잠시 트래킹을 중단하고 다음 작업을 수행합니다:\n1. VTube Studio 정상 종료\n2. Windows 그래픽 기본 설정 저장\n3. VTube Studio 실행\n4. VTSFloat_Meow 자동 재시작\n\n계속할까요?", L"\n\nОтслеживание ненадолго прервётся. Программа:\n1. Корректно закроет VTube Studio\n2. Сохранит графические настройки Windows\n3. Запустит VTube Studio\n4. Автоматически перезапустит VTSFloat_Meow\n\nПродолжить?"},
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
    constexpr std::pair<std::wstring_view, std::wstring_view> editorTerms[] = {
        {L"内部阴影", L"內陰影"}, {L"外部阴影", L"陰影"}, {L"阴影扩展", L"陰影擴展"},
        {L"参考背景像素", L"取樣背景色"}, {L"黑场", L"黑點"}, {L"白场", L"白點"},
        {L"图像", L"影像"}, {L"正片叠底", L"色彩增值"}, {L"颜色加深", L"加深顏色"},
        {L"颜色减淡", L"加亮顏色"}, {L"线性减淡（添加）", L"線性加亮（增加）"},
        {L"深色", L"顏色變暗"}, {L"实色混合", L"實色疊印混合"},
        {L"渐变网格", L"漸層網格"}, {L"边缘羽化", L"邊緣羽化"},
    };
    for (const auto& [source, translated] : editorTerms) {
        if (source == simplified) return std::wstring(translated);
    }
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
        {std::wstring_view(L"分辨率"), std::wstring_view(L"解析度")},
        {std::wstring_view(L"裁剪"), std::wstring_view(L"裁切")},
        {std::wstring_view(L"鎖定比例"), std::wstring_view(L"鎖定長寬比")},
        {std::wstring_view(L"比例已鎖定"), std::wstring_view(L"長寬比已鎖定")},
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

    const auto path = vtsfloat::CacheDirectory() / L"VTSFloat_Meow.custom-language.ini";
    const auto legacy = path.parent_path().parent_path() / path.filename();
    MoveFileExW(legacy.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH);
    std::error_code error;
    if (std::filesystem::exists(path, error) || WriteCustomLanguageTemplate(path)) {
        gCustomLanguagePath = path;
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
    static const auto translationIndex = [] {
        std::unordered_map<std::wstring_view, size_t> index;
        index.reserve(ARRAYSIZE(kTranslations));
        for (size_t i = 0; i < ARRAYSIZE(kTranslations); ++i) index.emplace(kTranslations[i].zh, i);
        return index;
    }();
    const auto found = translationIndex.find(simplifiedChinese);
    if (found != translationIndex.end()) {
        const auto index = found->second;
        const auto& entry = kTranslations[index];
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
