#include <QActionGroup>
#include "courtroom.h"
#include "options.h"
#include "file_functions.h"
#include "ulta_version.h"
#include "widgets/ulta_palette_dialog.h"
#include <QShortcut>
#include <QWheelEvent>
#include "widgets/ulta_narrator_dialog.h"
#include "bass.h"

#include <QFrame>
#include <QProcess>
#include <QGridLayout>
#include <QScopeGuard>
#include <QPainter>
#include <cmath>
#include <QSettings>
#include <QStyledItemDelegate>

namespace {
// Draws a thin black line under every row of the music and area lists. A
// style sheet border on the rows would hide their status colors instead.
class UltaRowLineDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override
  {
    QStyledItemDelegate::paint(painter, option, index);
    painter->save();
    painter->setPen(Qt::black);
    painter->drawLine(option.rect.left(), option.rect.bottom(),
                      option.rect.right(), option.rect.bottom());
    painter->restore();
  }
};
} // namespace

Courtroom::Courtroom(AOApplication *p_ao_app) : QMainWindow()
{
  ao_app = p_ao_app;

  this->setWindowFlags((this->windowFlags() | Qt::CustomizeWindowHint) &
                       ~Qt::WindowMaximizeButtonHint);

  ao_app->initBASS();
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0) // Needed for pre-5.10 RNG stuff
  qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch() / 1000));
#endif

  keepalive_timer = new QTimer(this);
  keepalive_timer->start(45000);

  chat_tick_timer = new QTimer(this);

  text_delay_timer = new QTimer(this);
  text_delay_timer->setSingleShot(true);

  text_queue_timer = new QTimer(this);
  text_queue_timer->setSingleShot(true);

  sfx_delay_timer = new QTimer(this);
  sfx_delay_timer->setSingleShot(true);

  music_player = new AOMusicPlayer(this, ao_app);
  music_player->set_muted(true);
  connect(&music_player->music_watcher, &QFutureWatcher<QString>::finished,
          this, &Courtroom::update_ui_music_name, Qt::QueuedConnection);

  sfx_player = new AOSfxPlayer(this, ao_app);
  sfx_player->set_muted(true);

  objection_player = new AOSfxPlayer(this, ao_app);
  objection_player->set_muted(true);

  blip_player = new AOBlipPlayer(this, ao_app);
  blip_player->set_muted(true);

  modcall_player = new AOSfxPlayer(this, ao_app);
  modcall_player->set_volume(50);

  ui_background = new AOImage(this, ao_app);
  ui_background->setObjectName("ui_background");

  ui_viewport = new QWidget(this);
  ui_viewport->setObjectName("ui_viewport");
  ui_vp_background = new BackgroundLayer(ui_viewport, ao_app);
  ui_vp_background->setObjectName("ui_vp_background");
  ui_vp_speedlines = new SplashLayer(ui_viewport, ao_app);
  ui_vp_speedlines->setObjectName("ui_vp_speedlines");
  ui_vp_speedlines->stretch = true;
  ui_vp_player_char = new CharLayer(ui_viewport, ao_app);
  ui_vp_player_char->setObjectName("ui_vp_player_char");
  ui_vp_player_char->masked = false;
  ui_vp_sideplayer_char = new CharLayer(ui_viewport, ao_app);
  ui_vp_sideplayer_char->setObjectName("ui_vp_sideplayer_char");
  ui_vp_sideplayer_char->masked = false;
  ui_vp_sideplayer_char->hide();
  // Second pair partner (stays hidden unless the server is ULTA-enabled)
  ui_vp_sideplayer2_char = new CharLayer(ui_viewport, ao_app);
  ui_vp_sideplayer2_char->setObjectName("ui_vp_sideplayer2_char");
  ui_vp_sideplayer2_char->masked = false;
  ui_vp_sideplayer2_char->hide();
  ui_vp_desk = new BackgroundLayer(ui_viewport, ao_app);
  ui_vp_desk->setObjectName("ui_vp_desk");

  ui_vp_effect = new EffectLayer(this, ao_app);
  ui_vp_effect->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_vp_effect->setObjectName("ui_vp_effect");

  ui_vp_evidence_display = new AOEvidenceDisplay(ui_viewport, ao_app);
  // Narrator tool animation (menu, pointer, click), above the desk
  ui_vp_tool_overlay = new CharLayer(ui_viewport, ao_app);
  ui_vp_tool_overlay->setObjectName("ui_vp_tool_overlay");
  ui_vp_tool_overlay->masked = false;
  ui_vp_tool_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_vp_tool_overlay->hide();
  // Blackout/whiteout layer, a sibling of the viewport so a flashback does not tint it
  ui_vp_fade = new UltaFadeOverlay(this);
  ui_vp_evidence_display->setObjectName("ui_vp_evidence_display");

  ui_vp_chatbox = new AOImage(this, ao_app);
  ui_vp_chatbox->setObjectName("ui_vp_chatbox");

  ui_vp_sticker = new StickerLayer(this, ao_app);
  ui_vp_sticker->set_play_once(false);
  ui_vp_sticker->set_cull_image(false);
  ui_vp_sticker->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_vp_sticker->setObjectName("ui_vp_sticker");

  ui_vp_showname = new QLabel(ui_vp_chatbox);
  ui_vp_showname->setObjectName("ui_vp_showname");
  ui_vp_showname->setAlignment(Qt::AlignLeft);
  ui_vp_chat_arrow = new InterfaceLayer(this, ao_app);
  ui_vp_chat_arrow->set_play_once(false);
  ui_vp_chat_arrow->set_cull_image(false);
  ui_vp_chat_arrow->setObjectName("ui_vp_chat_arrow");

  ui_vp_message = new QTextEdit(this);
  ui_vp_message->setFrameStyle(QFrame::NoFrame);
  ui_vp_message->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui_vp_message->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui_vp_message->setReadOnly(true);
  ui_vp_message->setObjectName("ui_vp_message");

  ui_vp_testimony = new SplashLayer(this, ao_app);
  ui_vp_testimony->set_play_once(false);
  ui_vp_testimony->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_vp_testimony->setObjectName("ui_vp_testimony");
  ui_vp_wtce = new SplashLayer(this, ao_app);
  ui_vp_wtce->set_play_once(true);
  ui_vp_wtce->continuous = false;
  ui_vp_wtce->force_continuous = true;
  ui_vp_wtce->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_vp_wtce->setObjectName("ui_vp_wtce");
  // Inside the viewport, so a flashback look tints the shouts too
  ui_vp_objection = new SplashLayer(ui_viewport, ao_app);
  ui_vp_objection->set_play_once(true);
  ui_vp_objection->continuous = false;
  ui_vp_objection->force_continuous = true;
  ui_vp_objection->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_vp_objection->setObjectName("ui_vp_objection");

  ui_ic_chatlog = new QTextEdit(this);
  ui_ic_chatlog->setReadOnly(true);
  ui_ic_chatlog->setObjectName("ui_ic_chatlog");
  ui_ic_chatlog->viewport()->installEventFilter(this); // no Ctrl+wheel zoom

  // Ctrl+F search in the IC log
  ui_find_bar = new UltaFindBar(ui_ic_chatlog, this);
  QShortcut *find_shortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
  connect(find_shortcut, &QShortcut::activated, this, [this] {
    if (!ui_char_select_background->isHidden())
      return;
    if (ui_find_bar->isVisible())
      ui_find_bar->closeBar();
    else
      ui_find_bar->openBar();
  });
  connect(ui_find_bar, &UltaFindBar::opened, this, &Courtroom::position_find_bar);
  connect(ui_find_bar, &UltaFindBar::closed, this, &Courtroom::position_find_bar);
  connect(ui_find_bar, &UltaFindBar::closed, this, [this] { ui_ic_chat_message->setFocus(); });

  log_maximum_blocks = Options::getInstance().maxLogSize();
  log_goes_downwards = Options::getInstance().logDirectionDownwards();
  log_colors = Options::getInstance().colorLogEnabled();
  log_newline = Options::getInstance().logNewline();
  log_margin = Options::getInstance().logMargin();
  log_timestamp = Options::getInstance().logTimestampEnabled();
  log_timestamp_format = Options::getInstance().logTimestampFormat();

  ui_debug_log = new AOTextArea(this, Options::getInstance().maxLogSize());
  ui_debug_log->setReadOnly(true);
  ui_debug_log->setOpenExternalLinks(true);
  ui_debug_log->hide();
  ui_debug_log->setObjectName("ui_debug_log");
  // Log text arrives queued and is written in small batches, so a burst cannot freeze the UI
  m_debug_flush = new QTimer(this);
  m_debug_flush->setSingleShot(true);
  connect(m_debug_flush, &QTimer::timeout, this, &Courtroom::flush_debug_log);
  connect(ao_app, &AOApplication::qt_log_text, this,
          &Courtroom::debug_message_handler, Qt::QueuedConnection);

  ui_server_chatlog = new AOTextArea(this);
  ui_server_chatlog->setReadOnly(true);
  ui_server_chatlog->setOpenExternalLinks(true);
  ui_server_chatlog->setObjectName("ui_server_chatlog");

  ui_area_list = new QTreeWidget(this);
  ui_area_list->setColumnCount(2);
  ui_area_list->hideColumn(0);
  ui_area_list->setHeaderHidden(true);
  ui_area_list->header()->setStretchLastSection(false);
  ui_area_list->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  ui_area_list->hide();
  ui_area_list->setObjectName("ui_area_list");
  ui_area_list->setItemDelegate(new UltaRowLineDelegate(ui_area_list));

  ui_music_list = new QTreeWidget(this);
  ui_music_list->setColumnCount(2);
  ui_music_list->hideColumn(1);
  ui_music_list->setHeaderHidden(true);
  ui_music_list->header()->setStretchLastSection(false);
  ui_music_list->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  ui_music_list->setContextMenuPolicy(Qt::CustomContextMenu);
  ui_music_list->setUniformRowHeights(true);
  ui_music_list->setObjectName("ui_music_list");
  ui_music_list->setItemDelegate(new UltaRowLineDelegate(ui_music_list));

  ui_music_display = new InterfaceLayer(this, ao_app);
  ui_music_display->set_play_once(false);
  ui_music_display->set_cull_image(false);
  ui_music_display->transform_mode = Qt::SmoothTransformation;
  ui_music_display->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_music_display->setObjectName("ui_music_display");

  ui_music_name = new ScrollText(ui_music_display);
  ui_music_name->setText(tr("None"));
  ui_music_name->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_music_name->setObjectName("ui_music_name");

  for (int i = 0; i < max_clocks; i++) {
    ui_clock[i] = new AOClockLabel(this);
    ui_clock[i]->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui_clock[i]->hide();
    ui_clock[i]->setObjectName("ui_clock" + QString::number(i));
  }

  ui_ic_chat_name = new QLineEdit(this);
  ui_ic_chat_name->setFrame(false);
  ui_ic_chat_name->setPlaceholderText(tr("Showname"));
  ui_ic_chat_name->setText(Options::getInstance().shownameOnJoin());
  ui_ic_chat_name->setObjectName("ui_ic_chat_name");

  ui_ic_chat_message = new QLineEdit(this);
  ui_ic_chat_message->setFrame(false);
  ui_ic_chat_message->setPlaceholderText(tr("Message in-character"));
  ui_ic_chat_message_filter = new AOLineEditFilter();
  ui_ic_chat_message_filter->preserve_selection = true;
  ui_ic_chat_message->installEventFilter(ui_ic_chat_message_filter);
  ui_ic_chat_message->setObjectName("ui_ic_chat_message");

  ui_muted = new AOImage(ui_ic_chat_message, ao_app);
  ui_muted->hide();
  ui_muted->setObjectName("ui_muted");

  ui_ooc_chat_message = new QLineEdit(this);
  ui_ooc_chat_message->setFrame(false);
  ui_ooc_chat_message->setObjectName("ui_ooc_chat_message");
  ui_ooc_chat_message->setPlaceholderText(tr("Message out-of-character"));

  ui_ooc_chat_name = new QLineEdit(this);
  ui_ooc_chat_name->setFrame(false);
  ui_ooc_chat_name->setPlaceholderText(tr("Name"));
  ui_ooc_chat_name->setMaxLength(30);
  ui_ooc_chat_name->setText(Options::getInstance().username());
  ui_ooc_chat_name->setObjectName("ui_ooc_chat_name");

  // ui_area_password = new QLineEdit(this);
  // ui_area_password->setFrame(false);
  ui_music_search = new QLineEdit(this);
  ui_music_search->setFrame(false);
  ui_music_search->setPlaceholderText(tr("Search"));
  ui_music_search->setObjectName("ui_music_search");

  ui_pos_dropdown = new QComboBox(this);
  ui_pos_dropdown->setContextMenuPolicy(Qt::CustomContextMenu);
  ui_pos_dropdown->view()->setTextElideMode(Qt::ElideLeft);
  ui_pos_dropdown->setObjectName("ui_pos_dropdown");

  ui_pos_remove = new AOButton(this, ao_app);
  ui_pos_remove->setObjectName("ui_pos_remove");

  ui_iniswap_dropdown = new QComboBox(this);
  ui_iniswap_dropdown->setContextMenuPolicy(Qt::CustomContextMenu);
  ui_iniswap_dropdown->view()->setTextElideMode(Qt::ElideLeft);
  ui_iniswap_dropdown->setObjectName("ui_iniswap_dropdown");

  ui_iniswap_remove = new AOButton(this, ao_app);
  ui_iniswap_remove->setObjectName("ui_iniswap_remove");

  ui_sfx_dropdown = new QComboBox(this);
  ui_sfx_dropdown->setContextMenuPolicy(Qt::CustomContextMenu);
  ui_sfx_dropdown->view()->setTextElideMode(Qt::ElideLeft);
  ui_sfx_dropdown->setObjectName("ui_sfx_dropdown");

  ui_sfx_remove = new AOButton(this, ao_app);
  ui_sfx_remove->setObjectName("ui_sfx_remove");

  ui_effects_dropdown = new QComboBox(this);
  ui_effects_dropdown->view()->setTextElideMode(Qt::ElideLeft);
  ui_effects_dropdown->setContextMenuPolicy(Qt::CustomContextMenu);
  ui_effects_dropdown->setObjectName("ui_effects_dropdown");

  ui_defense_bar = new AOImage(this, ao_app);
  ui_defense_bar->setObjectName("ui_defense_bar");

  ui_prosecution_bar = new AOImage(this, ao_app);
  ui_prosecution_bar->setObjectName("ui_prosecution_bar");

  ui_music_label = new QLabel(this);
  ui_music_label->setObjectName("ui_music_label");

  ui_sfx_label = new QLabel(this);
  ui_sfx_label->setObjectName("ui_sfx_label");

  ui_blip_label = new QLabel(this);
  ui_blip_label->setObjectName("ui_blip_label");

  ui_hold_it = new AOButton(this, ao_app);
  ui_hold_it->setObjectName("ui_hold_it");

  ui_objection = new AOButton(this, ao_app);
  ui_objection->setObjectName("ui_objection");

  ui_take_that = new AOButton(this, ao_app);
  ui_take_that->setObjectName("ui_take_that");

  ui_ooc_toggle = new AOButton(this, ao_app);
  ui_ooc_toggle->setObjectName("ui_ooc_toggle");

  ui_witness_testimony = new AOButton(this, ao_app);
  ui_witness_testimony->setObjectName("ui_witness_testimony");

  ui_cross_examination = new AOButton(this, ao_app);
  ui_cross_examination->setObjectName("ui_cross_examination");

  ui_guilty = new AOButton(this, ao_app);
  ui_guilty->setObjectName("ui_guilty");

  ui_not_guilty = new AOButton(this, ao_app);
  ui_not_guilty->setObjectName("ui_not_guilty");

  ui_change_character = new AOButton(this, ao_app);
  ui_change_character->setObjectName("ui_change_character");

  ui_reload_theme = new AOButton(this, ao_app);
  ui_reload_theme->setObjectName("ui_reload_theme");

  ui_call_mod = new AOButton(this, ao_app);
  ui_call_mod->setObjectName("ui_call_mod");

  ui_settings = new AOButton(this, ao_app);
  ui_settings->setObjectName("ui_settings");

  ui_switch_area_music = new AOButton(this, ao_app);
  ui_switch_area_music->setObjectName("ui_switch_area_music");

  ui_pre = new QCheckBox(this);
  ui_pre->setText(tr("Pre"));
  ui_pre->setObjectName("ui_pre");

  ui_flip = new QCheckBox(this);
  ui_flip->setText(tr("Flip"));
  ui_flip->hide();
  ui_flip->setObjectName("ui_flip");

  ui_guard = new QCheckBox(this);
  ui_guard->setText(tr("Guard"));
  ui_guard->hide();
  ui_guard->setObjectName("ui_guard");

  ui_additive = new QCheckBox(this);
  ui_additive->setText(tr("Additive"));
  ui_additive->hide();
  ui_additive->setObjectName("ui_additive");

  ui_showname_enable = new QCheckBox(this);
  ui_showname_enable->setChecked(Options::getInstance().customShownameEnabled());
  ui_showname_enable->setText(tr("Shownames"));
  ui_showname_enable->setObjectName("ui_showname_enable");

  ui_immediate = new QCheckBox(this);
  ui_immediate->setText(tr("Immediate"));
  ui_immediate->hide();
  ui_immediate->setObjectName("ui_immediate");

  ui_custom_objection = new AOButton(this, ao_app);
  ui_custom_objection->setContextMenuPolicy(Qt::CustomContextMenu);
  ui_custom_objection->setObjectName("ui_custom_objection");

  custom_obj_menu = new QMenu(this);
  custom_obj_menu->setObjectName("ui_custom_obj_menu");

  ui_realization = new AOButton(this, ao_app);
  ui_realization->setObjectName("ui_realization");

  ui_screenshake = new AOButton(this, ao_app);
  ui_screenshake->setObjectName("ui_screenshake");

  ui_mute = new AOButton(this, ao_app);
  ui_mute->setObjectName("ui_mute");

  ui_defense_plus = new AOButton(this, ao_app);
  ui_defense_plus->setObjectName("ui_defense_plus");

  ui_defense_minus = new AOButton(this, ao_app);
  ui_defense_minus->setObjectName("ui_defense_minus");

  ui_prosecution_plus = new AOButton(this, ao_app);
  ui_prosecution_plus->setObjectName("ui_prosecution_plus");

  ui_prosecution_minus = new AOButton(this, ao_app);
  ui_prosecution_minus->setObjectName("ui_prosecution_minus");

  ui_text_color = new QComboBox(this);
  ui_text_color->setContextMenuPolicy(Qt::CustomContextMenu);
  ui_text_color->setObjectName("ui_text_color");

  ui_music_slider = new QSlider(Qt::Horizontal, this);
  ui_music_slider->setRange(0, 100);
  ui_music_slider->setValue(Options::getInstance().musicVolume());
  ui_music_slider->setObjectName("ui_music_slider");

  ui_sfx_slider = new QSlider(Qt::Horizontal, this);
  ui_sfx_slider->setRange(0, 100);
  ui_sfx_slider->setValue(Options::getInstance().sfxVolume());
  ui_sfx_slider->setObjectName("ui_sfx_slider");

  ui_blip_slider = new QSlider(Qt::Horizontal, this);
  ui_blip_slider->setRange(0, 100);
  ui_blip_slider->setValue(Options::getInstance().blipVolume());
  ui_blip_slider->setObjectName("ui_blip_slider");

  ui_mute_list = new QListWidget(this);
  ui_mute_list->setObjectName("ui_mute_list");

  ui_pair_list = new QListWidget(this);
  ui_pair_list->setObjectName("ui_pair_list");

  ui_pair_offset_spinbox = new QSpinBox(this);
  ui_pair_offset_spinbox->setRange(-100, 100);
  ui_pair_offset_spinbox->setSuffix("% x");
  ui_pair_offset_spinbox->setObjectName("ui_pair_offset_spinbox");

  ui_pair_vert_offset_spinbox = new QSpinBox(this);
  ui_pair_vert_offset_spinbox->setRange(-100, 100);
  ui_pair_vert_offset_spinbox->setSuffix("% y");
  ui_pair_vert_offset_spinbox->setObjectName("ui_pair_vert_offset_spinbox");

  ui_pair_order_dropdown = new QComboBox(this);
  ui_pair_order_dropdown->addItem(tr("To front"));
  ui_pair_order_dropdown->addItem(tr("To behind"));
  ui_pair_order_dropdown->setObjectName("ui_pair_order_dropdown");

  // Picks which pair partner (1 or 2) the character list edits
  ui_pair_slot_dropdown = new QComboBox(this);
  ui_pair_slot_dropdown->addItem(tr("Partner 1"));
  ui_pair_slot_dropdown->addItem(tr("Partner 2"));
  ui_pair_slot_dropdown->setObjectName("ui_pair_slot_dropdown");
  ui_pair_slot_dropdown->hide();

  // Custom text colors button (shown only on ULTA-enabled servers)
  ui_ulta_colors = new QPushButton(tr("Aa"), this);
  ui_ulta_colors->setObjectName("ui_ulta_colors");
  ui_ulta_colors->setToolTip(tr("Custom text colors: choose your own colors "
                                "for the text symbols, with hotslots."));
  ui_ulta_colors->hide();
  load_ulta_color_settings();

  // Narrator mode toggle (right-click opens its settings)
  ui_ulta_narrator = new QPushButton(tr("Nr"), this);
  ui_ulta_narrator->setObjectName("ui_ulta_narrator");
  ui_ulta_narrator->setCheckable(true);
  ui_ulta_narrator->setContextMenuPolicy(Qt::CustomContextMenu);
  ui_ulta_narrator->hide();
  load_ulta_narrator_settings();

  // One-shot fade of the player's own character (click for the menu)
  ui_ulta_charfade = new QPushButton(tr("Fd"), this);
  ui_ulta_charfade->setObjectName("ui_ulta_charfade");
  ui_ulta_charfade->setCheckable(true);
  ui_ulta_charfade->hide();
  connect(ui_ulta_charfade, &QPushButton::clicked, this,
          &Courtroom::on_charfade_clicked);
  ui_viewport->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui_viewport, &QWidget::customContextMenuRequested, this,
          &Courtroom::on_viewport_context_menu);

  // Narrator panel, floats over the lower left of the viewport (theme key `ulta_narrator_panel`)
  ui_narrator_panel = new UltaDragPanel(this);
  ui_narrator_panel->setObjectName("ui_narrator_panel");
  ui_narrator_panel->setToolTip(tr("Drag the panel to move it."));
  connect(ui_narrator_panel, &UltaDragPanel::dragFinished, this,
          [this](const QPoint &p) {
            ulta_narrator_panel_has_pos = true;
            ulta_narrator_panel_pos = p;
            save_ulta_narrator_settings();
          });
  ui_narrator_panel->setStyleSheet(QStringLiteral(
      "QFrame#ui_narrator_panel { background-color: rgba(20, 24, 40, 235); "
      "border: 1px solid #c8b878; }"
      "QLabel, QCheckBox { color: #f0f0f0; background: transparent; }"
      "QPushButton { color: #101010; background-color: #e4e4e4; padding: 2px 6px; "
      "border: 1px solid #808080; }"
      "QPushButton:checked { background-color: #c8b878; }"
      "QPushButton:hover { background-color: #ffffff; }"
      "QComboBox, QLineEdit { color: #101010; background-color: #ffffff; }"
      "QComboBox QAbstractItemView { color: #101010; background-color: "
      "#ffffff; selection-background-color: #3a6ea5; selection-color: "
      "#ffffff; }"));
  {
    QGridLayout *narr_layout = new QGridLayout(ui_narrator_panel);
    narr_layout->setContentsMargins(8, 6, 8, 6);
    narr_layout->setSpacing(4);

    QLabel *narr_title = new QLabel(tr("Narrator"), ui_narrator_panel);
    QFont narr_font = narr_title->font();
    narr_font.setBold(true);
    narr_title->setFont(narr_font);
    narr_layout->addWidget(narr_title, 0, 0, 1, 2);

    ui_narrator_scene = new QComboBox(ui_narrator_panel);
    ui_narrator_scene->addItem(tr("Over my character"), 0);
    ui_narrator_scene->addItem(tr("Alone (hide characters)"), 1);
    ui_narrator_scene->setToolTip(
        tr("Over my character: your character stays on screen while the "
           "narrator speaks.\nAlone: only the background and the narration."));
    narr_layout->addWidget(new QLabel(tr("Scene:"), ui_narrator_panel), 1, 0);
    narr_layout->addWidget(ui_narrator_scene, 1, 1);

    ui_narrator_voice = new QComboBox(ui_narrator_panel);
    ui_narrator_voice->addItem(tr("Neutral"), "NarratorULTA");
    ui_narrator_voice->addItem(tr("Male"), "NarratorULTAMale");
    ui_narrator_voice->addItem(tr("Female"), "NarratorULTAFemale");
    ui_narrator_voice->setToolTip(tr("The narrator's voice (text blips)."));
    narr_layout->addWidget(new QLabel(tr("Voice:"), ui_narrator_panel), 2, 0);
    narr_layout->addWidget(ui_narrator_voice, 2, 1);

    ui_narrator_name = new QLineEdit(ui_narrator_panel);
    ui_narrator_name->setMaxLength(24);
    ui_narrator_name->setToolTip(
        tr("The name shown on the chatbox while narrating."));
    narr_layout->addWidget(new QLabel(tr("Name:"), ui_narrator_panel), 3, 0);
    narr_layout->addWidget(ui_narrator_name, 3, 1);

    // ---- Scene tools (collapsed by default) ----
    ui_narrator_tools_toggle =
        new QPushButton(tr("Scene tools >>"), ui_narrator_panel);
    ui_narrator_tools_toggle->setCheckable(true);
    ui_narrator_tools_toggle->setToolTip(
        tr("Fades, the investigation menu and sound cues. Click a tool to "
           "select it, then type a line (or leave it empty) and press Enter "
           "to play it for everyone in the room. They change what everyone "
           "sees; the server decides where they are allowed."));
    narr_layout->addWidget(ui_narrator_tools_toggle, 4, 0, 1, 2);

    ui_narrator_tools = new QWidget(ui_narrator_panel);
    QGridLayout *tools_layout = new QGridLayout(ui_narrator_tools);
    tools_layout->setContentsMargins(0, 2, 0, 0);
    tools_layout->setSpacing(3);

    // Each tool plays one emote of the narrator character
    struct ToolDef {
      QString label;
      int emote;
      QString tip;
    };
    struct ToolGroup {
      QString title;
      QList<ToolDef> tools;
    };
    const QList<ToolGroup> tool_groups = {
        {tr("Fade"),
         {{tr("To black"), -1, tr("Fade the scene to black.")},
          {tr("From black"), -2, tr("Fade back in from black.")},
          {tr("To white"), -3, tr("Fade the scene to white.")},
          {tr("From white"), -4, tr("Fade back in from white.")}}},
        {tr("Sound"),
         {{tr("Dink"), 2, tr("A short chime.")},
          {tr("Save"), 3, tr("The save sound.")},
          {tr("Select"), 4, tr("The select sound.")}}},
        {tr("Menu"),
         {{tr("Open"), 7, tr("Open the investigation menu.")},
          {tr("Examine"), 8, tr("Highlight Examine.")},
          {tr("Converse"), 9, tr("Highlight Converse.")},
          {tr("Move"), 10, tr("Highlight Move.")},
          {tr("Present"), 11, tr("Highlight Present.")},
          {tr("Close"), 16, tr("Close the investigation menu.")}}},
        {tr("Click"),
         {{tr("Examine"), 12, tr("Click Examine.")},
          {tr("Converse"), 13, tr("Click Converse.")},
          {tr("Move"), 14, tr("Click Move.")},
          {tr("Present"), 15, tr("Click Present.")}}},
        {tr("Pointer"),
         {{tr("Point"), 17, tr("Show the pointer.")},
          {tr("Select"), 18, tr("Pointer over a selectable item.")},
          {tr("Checked"), 19, tr("Pointer over an item already checked.")},
          {tr("Choose"), 20, tr("The pointer chooses.")}}},
    };
    int tools_row = 0;
    for (const ToolGroup &group : tool_groups) {
      tools_layout->addWidget(new QLabel(group.title, ui_narrator_tools),
                              tools_row, 0);
      QHBoxLayout *tools_buttons = new QHBoxLayout();
      tools_buttons->setSpacing(3);
      for (const ToolDef &tool : group.tools) {
        QPushButton *tool_button = new QPushButton(tool.label, ui_narrator_tools);
        tool_button->setToolTip(tool.tip);
        const int emote = tool.emote;
        tool_button->setCheckable(true);
        tool_button->setProperty("emote", emote);
        m_tool_buttons.append(tool_button);
        connect(tool_button, &QPushButton::clicked, this,
                [this, emote]() { arm_scene_tool(emote); });
        tools_buttons->addWidget(tool_button);
      }
      tools_buttons->addStretch(1);
      tools_layout->addLayout(tools_buttons, tools_row, 1);
      ++tools_row;
    }
    ui_narrator_hold =
        new QCheckBox(tr("Hold the fade until the fade back in"), ui_narrator_tools);
    ui_narrator_hold->setToolTip(
        tr("Off: the scene returns as soon as a different character speaks.\n"
           "On: a fade to black/white stays until someone fades back in or the "
           "area changes, even while other characters speak."));
    tools_layout->addWidget(ui_narrator_hold, tools_row, 0, 1, 2);
    ui_narrator_tools->hide();
    narr_layout->addWidget(ui_narrator_tools, 5, 0, 1, 2);
    connect(ui_narrator_tools_toggle, &QPushButton::toggled, this,
            [this](bool open) {
              ui_narrator_tools->setVisible(open);
              ui_narrator_tools_toggle->setText(
                  open ? tr("Scene tools <<") : tr("Scene tools >>"));
              ui_narrator_panel->adjustSize();
              ui_narrator_panel->clampToParent();
            });
    narr_layout->setColumnStretch(1, 1);
  }
  ui_narrator_panel->hide();

  // Flashback button and panel
  // Status strip: area, CMs and active scene effects
  ui_ulta_status = new QLabel(this);
  ui_ulta_status->setObjectName("ui_ulta_status");
  ui_ulta_status->setTextFormat(Qt::RichText);
  ui_ulta_status->setAttribute(Qt::WA_TransparentForMouseEvents);
  ui_ulta_status->hide();
  ui_ulta_flash = new QPushButton(tr("Fb"), this);
  ui_ulta_flash->setObjectName("ui_ulta_flash");
  ui_ulta_flash->setCheckable(true);
  ui_ulta_flash->setToolTip(tr("Flashback: open the flashback panel"));
  ui_ulta_flash->hide();
  ui_flash_panel = new UltaFlashbackPanel(this);
  ui_flash_panel->hide();
  {
    QSettings settings(get_base_path() + "ulta.ini", QSettings::IniFormat);
    settings.beginGroup("flashback");
    m_flash_panel_has_pos =
        settings.contains("panel_x") && settings.contains("panel_y");
    if (m_flash_panel_has_pos)
      m_flash_panel_pos = QPoint(settings.value("panel_x").toInt(),
                                 settings.value("panel_y").toInt());
    settings.endGroup();
  }
  connect(ui_ulta_flash, &QPushButton::toggled, this,
          [this](bool) { sync_flash_panel(); });
  connect(ui_flash_panel, &UltaDragPanel::dragFinished, this,
          [this](const QPoint &p) {
            m_flash_panel_has_pos = true;
            m_flash_panel_pos = p;
            QSettings settings(get_base_path() + "ulta.ini",
                               QSettings::IniFormat);
            settings.beginGroup("flashback");
            settings.setValue("panel_x", p.x());
            settings.setValue("panel_y", p.y());
            settings.endGroup();
            settings.sync();
          });
  connect(ui_flash_panel, &UltaFlashbackPanel::startRequested, this,
          [this](const QString &look, bool hold) {
            ao_app->send_server_packet(
                new AOPacket("FB", {look, hold ? "1" : "0"}));
          });
  connect(ui_flash_panel, &UltaFlashbackPanel::endRequested, this, [this]() {
    ao_app->send_server_packet(new AOPacket("FB", {"-", "0"}));
  });
  update_narrator_button();

  ui_pair_button = new AOButton(this, ao_app);
  ui_pair_button->setObjectName("ui_pair_button");

  ui_evidence_button = new AOButton(this, ao_app);
  ui_evidence_button->setContextMenuPolicy(Qt::CustomContextMenu);
  ui_evidence_button->setObjectName("ui_evidence_button");

  initialize_emotes();
  initialize_evidence();
  ui_evidence->installEventFilter(this); // follow the panel's show/hide
  for (QPushButton *button : {ui_ulta_colors, ui_ulta_narrator, ui_ulta_charfade, ui_ulta_flash})
    if (button)
      button->setFocusPolicy(Qt::TabFocus);

  // TODO : Properly handle widget creation order.
  // Good enough for 2.10
  ui_pair_list->raise();

  construct_char_select();

  connect(keepalive_timer, &QTimer::timeout, this, &Courtroom::ping_server);

  connect(ui_vp_objection, &SplashLayer::done, this, &Courtroom::objection_done);
  connect(ui_vp_player_char, &CharLayer::done, this, &Courtroom::preanim_done);
  connect(ui_vp_tool_overlay, &CharLayer::done, this, &Courtroom::tool_overlay_done);
  connect(ui_vp_player_char, &CharLayer::shake, this, &Courtroom::do_screenshake);
  connect(ui_vp_player_char, &CharLayer::flash, this, &Courtroom::do_flash);
  connect(ui_vp_player_char, &CharLayer::play_sfx, this,
          &Courtroom::play_char_sfx);

  connect(text_delay_timer, &QTimer::timeout, this,
          &Courtroom::start_chat_ticking);

  connect(text_queue_timer, &QTimer::timeout, this,
          &Courtroom::chatmessage_dequeue);

  m_wtce_timer = new QTimer(this);
  m_wtce_timer->setSingleShot(true);
  connect(m_wtce_timer, &QTimer::timeout, this, &Courtroom::wtce_finished);
  connect(ui_vp_wtce, &AOLayer::done, this, &Courtroom::wtce_finished);

  connect(sfx_delay_timer, &QTimer::timeout, this, &Courtroom::play_sfx);

  connect(chat_tick_timer, &QTimer::timeout, this, &Courtroom::chat_tick);

  connect(ui_pos_dropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          QOverload<int>::of(&Courtroom::on_pos_dropdown_changed));
  connect(ui_pos_dropdown, &QComboBox::editTextChanged, this,
          QOverload<QString>::of(&Courtroom::on_pos_dropdown_changed));
  connect(ui_pos_dropdown, &QComboBox::customContextMenuRequested, this,
          &Courtroom::on_pos_dropdown_context_menu_requested);
  connect(ui_pos_remove, &AOButton::clicked, this, &Courtroom::on_pos_remove_clicked);

  connect(ui_iniswap_dropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &Courtroom::on_iniswap_dropdown_changed);
  connect(ui_iniswap_dropdown, &QComboBox::customContextMenuRequested, this,
          &Courtroom::on_iniswap_context_menu_requested);
  connect(ui_iniswap_remove, &AOButton::clicked, this,
          &Courtroom::on_iniswap_remove_clicked);

  connect(ui_sfx_dropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &Courtroom::on_sfx_dropdown_changed);
  connect(ui_sfx_dropdown, &QComboBox::editTextChanged, this,
          &Courtroom::on_sfx_dropdown_custom);
  connect(ui_sfx_dropdown, &QComboBox::customContextMenuRequested, this,
          &Courtroom::on_sfx_context_menu_requested);
  connect(ui_sfx_remove, &AOButton::clicked, this,
          &Courtroom::on_sfx_remove_clicked);

  connect(ui_effects_dropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &Courtroom::on_effects_dropdown_changed);
  connect(ui_effects_dropdown, &QComboBox::customContextMenuRequested, this,
          &Courtroom::on_effects_context_menu_requested);

  connect(ui_music_search, &QLineEdit::returnPressed, this,
          &Courtroom::on_music_search_return_pressed);
  connect(ui_mute_list, &QListWidget::clicked, this,
          &Courtroom::on_mute_list_clicked);

  connect(ui_ic_chat_message, &QLineEdit::returnPressed, this,
          &Courtroom::on_chat_return_pressed);

  connect(ui_ooc_chat_message, &QLineEdit::returnPressed, this,
          &Courtroom::on_ooc_return_pressed);

  connect(ui_music_list, &QTreeWidget::itemDoubleClicked,
          this, &Courtroom::on_music_list_double_clicked);
  connect(ui_music_list, &QTreeWidget::customContextMenuRequested, this,
          &Courtroom::on_music_list_context_menu_requested);

  connect(ui_area_list, &QTreeWidget::itemDoubleClicked, this,
          &Courtroom::on_area_list_double_clicked);

  connect(ui_hold_it, &AOButton::clicked, this, &Courtroom::on_hold_it_clicked);
  connect(ui_objection, &AOButton::clicked, this, &Courtroom::on_objection_clicked);
  connect(ui_take_that, &AOButton::clicked, this, &Courtroom::on_take_that_clicked);
  connect(ui_custom_objection, &AOButton::clicked, this,
          &Courtroom::on_custom_objection_clicked);
  connect(ui_custom_objection,
          &AOButton::customContextMenuRequested, this,
          &Courtroom::show_custom_objection_menu);

  connect(ui_realization, &AOButton::clicked, this,
          &Courtroom::on_realization_clicked);
  connect(ui_screenshake, &AOButton::clicked, this,
          &Courtroom::on_screenshake_clicked);

  connect(ui_mute, &AOButton::clicked, this, &Courtroom::on_mute_clicked);

  connect(ui_defense_minus, &AOButton::clicked, this,
          &Courtroom::on_defense_minus_clicked);
  connect(ui_defense_plus, &AOButton::clicked, this,
          &Courtroom::on_defense_plus_clicked);
  connect(ui_prosecution_minus, &AOButton::clicked, this,
          &Courtroom::on_prosecution_minus_clicked);
  connect(ui_prosecution_plus, &AOButton::clicked, this,
          &Courtroom::on_prosecution_plus_clicked);

  connect(ui_text_color, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &Courtroom::on_text_color_changed);
  connect(ui_text_color, &QComboBox::customContextMenuRequested, this,
          &Courtroom::on_text_color_context_menu_requested);

  // Keep the volumes as soon as they change. The stock client only saved them
  // when the courtroom was destroyed, so a crash or a killed process lost them.
  connect(ui_music_slider, &QSlider::valueChanged, this,
          [](int value) { Options::getInstance().setMusicVolume(value); });
  connect(ui_sfx_slider, &QSlider::valueChanged, this,
          [](int value) { Options::getInstance().setSfxVolume(value); });
  connect(ui_blip_slider, &QSlider::valueChanged, this,
          [](int value) { Options::getInstance().setBlipVolume(value); });
  connect(ui_music_slider, &QSlider::valueChanged, this,
          &Courtroom::on_music_slider_moved);
  connect(ui_sfx_slider, &QSlider::valueChanged, this,
          &Courtroom::on_sfx_slider_moved);
  connect(ui_blip_slider, &QSlider::valueChanged, this,
          &Courtroom::on_blip_slider_moved);

  connect(ui_ooc_toggle, &AOButton::clicked, this,
          &Courtroom::on_ooc_toggle_clicked);

  connect(ui_music_search, &QLineEdit::textChanged, this,
          &Courtroom::on_music_search_edited);

  connect(ui_witness_testimony, &AOButton::clicked, this,
          &Courtroom::on_witness_testimony_clicked);
  connect(ui_cross_examination, &AOButton::clicked, this,
          &Courtroom::on_cross_examination_clicked);
  connect(ui_guilty, &AOButton::clicked, this, &Courtroom::on_guilty_clicked);
  connect(ui_not_guilty, &AOButton::clicked, this,
          &Courtroom::on_not_guilty_clicked);

  connect(ui_change_character, &AOButton::clicked, this,
          &Courtroom::on_change_character_clicked);
  connect(ui_reload_theme, &AOButton::clicked, this,
          &Courtroom::on_reload_theme_clicked);
  connect(ui_call_mod, &AOButton::clicked, this, &Courtroom::on_call_mod_clicked);
  connect(ui_settings, &AOButton::clicked, this, &Courtroom::on_settings_clicked);
  connect(ui_switch_area_music, &AOButton::clicked, this,
          &Courtroom::on_switch_area_music_clicked);

  connect(ui_pre, &AOButton::clicked, this, &Courtroom::on_pre_clicked);
  connect(ui_flip, &AOButton::clicked, this, &Courtroom::on_flip_clicked);
  connect(ui_additive, &AOButton::clicked, this, &Courtroom::on_additive_clicked);
  connect(ui_guard, &AOButton::clicked, this, &Courtroom::on_guard_clicked);

  connect(ui_showname_enable, &AOButton::clicked, this,
          &Courtroom::on_showname_enable_clicked);

  connect(ui_pair_button, &AOButton::clicked, this, &Courtroom::on_pair_clicked);
  connect(ui_pair_list, &QListWidget::clicked, this,
          &Courtroom::on_pair_list_clicked);
  connect(ui_pair_offset_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &Courtroom::on_pair_offset_changed);
  connect(ui_pair_vert_offset_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &Courtroom::on_pair_vert_offset_changed);
  connect(ui_pair_order_dropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &Courtroom::on_pair_order_dropdown_changed);
  connect(ui_pair_slot_dropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &Courtroom::on_pair_slot_dropdown_changed);
  connect(ui_ulta_colors, &QPushButton::clicked, this,
          &Courtroom::on_ulta_colors_clicked);
  connect(ui_ulta_narrator, &QPushButton::toggled, this,
          &Courtroom::on_ulta_narrator_toggled);
  connect(ui_ulta_narrator, &QPushButton::customContextMenuRequested, this,
          &Courtroom::on_ulta_narrator_context_menu);
  connect(ui_narrator_scene, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &Courtroom::on_narrator_panel_changed);
  connect(ui_narrator_voice, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &Courtroom::on_narrator_panel_changed);
  connect(ui_narrator_name, &QLineEdit::editingFinished, this,
          &Courtroom::on_narrator_panel_changed);
  for (QSlider *volume_slider : {ui_music_slider, ui_sfx_slider, ui_blip_slider}) {
    volume_slider->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(volume_slider, &QSlider::customContextMenuRequested, this,
            &Courtroom::on_audio_slider_context_menu);
  }

  connect(ui_evidence_button, &AOButton::clicked, this,
          &Courtroom::on_evidence_button_clicked);
  connect(ui_evidence_button, &QComboBox::customContextMenuRequested, this,
          &Courtroom::on_evidence_context_menu_requested);

  connect(qApp, QOverload<Qt::ApplicationState>::of(&QApplication::applicationStateChanged), this,
          &Courtroom::on_application_state_changed);

  connect(ui_vp_evidence_display, &AOEvidenceDisplay::show_evidence_details, this, &Courtroom::show_evidence);

  set_widgets();

  set_char_select();
}

void Courtroom::on_application_state_changed(Qt::ApplicationState state)
{
  // After being away BASS may hold a dead output device, rebuild it
  if (state == Qt::ApplicationActive && !ao_app->audio_device_ok())
    reload_audio_device(false);

  // Unsuppressed
  suppress_audio = 0;
  if (state != Qt::ApplicationActive) {
    // Suppressed audio setting
    suppress_audio = Options::getInstance().defaultSuppressAudio();
  }
  update_audio_volume();
}

void Courtroom::update_audio_volume()
{
  float remaining_percent = 1.0f - static_cast<float>(suppress_audio / 100.0f);
  if (remaining_percent > 1)
    remaining_percent = 1;
  if (remaining_percent < 0)
    remaining_percent = 0;

  music_player->set_volume(ui_music_slider->value() * remaining_percent, 0); // set music
  // Set the ambience and other misc. music layers
  for (int i = 1; i < music_player->m_channelmax; ++i) {
    music_player->set_volume(ui_sfx_slider->value() * remaining_percent, i);
  }
  sfx_player->set_volume(ui_sfx_slider->value() * remaining_percent);
  objection_player->set_volume(ui_sfx_slider->value() * remaining_percent);
  blip_player->set_volume(ui_blip_slider->value() * remaining_percent);
}

void Courtroom::set_courtroom_size()
{
  QString filename = "courtroom_design.ini";
  pos_size_type f_courtroom =
      ao_app->get_element_dimensions("courtroom", filename);

  if (f_courtroom.width < 0 || f_courtroom.height < 0) {
    qWarning() << "did not find courtroom width or height in " << filename;

    this->setFixedSize(714, 668);
  }
  else {
    m_courtroom_width = f_courtroom.width;
    m_courtroom_height = f_courtroom.height;

    this->setFixedSize(f_courtroom.width, f_courtroom.height);
  }
  ui_background->move(0, 0);
  ui_background->resize(m_courtroom_width, m_courtroom_height);
  ui_background->set_image("courtroombackground");
}

void Courtroom::set_mute_list()
{
  mute_map.clear();

  // maps which characters are muted based on cid, none are muted by default
  for (int n_cid = 0; n_cid < char_list.size(); n_cid++) {
    mute_map.insert(n_cid, false);
  }

  QStringList sorted_mute_list;

  for (const char_type &i_char : qAsConst(char_list))
    sorted_mute_list.append(i_char.name);

  sorted_mute_list.sort();

  for (const QString &i_name : sorted_mute_list) {
    // mute_map.insert(i_name, false);
    ui_mute_list->addItem(i_name);
  }
}

void Courtroom::set_pair_list()
{
  QStringList sorted_pair_list;

  for (const char_type &i_char : qAsConst(char_list))
    sorted_pair_list.append(i_char.name);

  sorted_pair_list.sort();

  for (const QString &i_name : sorted_pair_list) {
    ui_pair_list->addItem(i_name);
  }
}

void Courtroom::set_widgets()
{
  QString filename = "courtroom_design.ini";

  set_fonts();
  set_size_and_pos(ui_viewport, "viewport");

  // If there is a point to it, show all CCCC features.
  // We also do this this soon so that set_size_and_pos can hide them all later,
  // if needed.
  if (ao_app->cccc_ic_supported) {
    ui_pair_button->show();
    ui_immediate->show();
    ui_showname_enable->show();
    ui_ic_chat_name->show();
    ui_ic_chat_name->setEnabled(true);
  }
  else {
    ui_pair_button->hide();
    ui_immediate->hide();
    ui_showname_enable->hide();
    ui_ic_chat_name->hide();
    ui_ic_chat_name->setEnabled(false);
  }

  // We also show the non-server-dependent client additions.
  // Once again, if the theme can't display it, set_move_and_pos will catch
  // them.
  ui_settings->show();

  // make the BG's reload
  ui_vp_background->kill();
  ui_vp_desk->kill();

  ui_vp_background->move_and_center(0, 0);
  ui_vp_background->combo_resize(ui_viewport->width(), ui_viewport->height());

  ui_vp_speedlines->move_and_center(0, 0);
  ui_vp_speedlines->combo_resize(ui_viewport->width(), ui_viewport->height());

  ui_vp_player_char->move_and_center(0, 0);
  ui_vp_player_char->combo_resize(ui_viewport->width(), ui_viewport->height());

  ui_vp_sideplayer_char->move_and_center(0, 0);
  ui_vp_sideplayer_char->combo_resize(ui_viewport->width(),
                                      ui_viewport->height());

  ui_vp_sideplayer2_char->move_and_center(0, 0);
  ui_vp_sideplayer2_char->combo_resize(ui_viewport->width(),
                                       ui_viewport->height());
  ui_vp_tool_overlay->move_and_center(0, 0);
  ui_vp_tool_overlay->combo_resize(ui_viewport->width(), ui_viewport->height());
  ui_vp_fade->setGeometry(ui_viewport->geometry());
  raise_scene_fade();

  // the AO2 desk element
  ui_vp_desk->move_and_center(0, 0);
  ui_vp_desk->combo_resize(ui_viewport->width(), ui_viewport->height());

  ui_vp_evidence_display->move(0, 0);
  ui_vp_evidence_display->combo_resize(ui_viewport->width(),
                                       ui_viewport->height());

  // layering shenanigans with ui_vp_chatbox prevent us from doing the sensible
  // thing, which is to parent these to ui_viewport. instead, AOLayer handles
  // masking so we don't overlap parts of the UI, and they become free floating
  // widgets.
  ui_vp_testimony->move_and_center(ui_viewport->x(), ui_viewport->y());
  ui_vp_testimony->combo_resize(ui_viewport->width(), ui_viewport->height());

  ui_vp_effect->move_and_center(ui_viewport->x(), ui_viewport->y());
  ui_vp_effect->combo_resize(ui_viewport->width(), ui_viewport->height());

  ui_vp_wtce->move_and_center(ui_viewport->x(), ui_viewport->y());
  ui_vp_wtce->combo_resize(ui_viewport->width(), ui_viewport->height());

  ui_vp_objection->move_and_center(0, 0);
  ui_vp_objection->combo_resize(ui_viewport->width(), ui_viewport->height());

  log_maximum_blocks = Options::getInstance().maxLogSize();

  bool regenerate = log_goes_downwards != Options::getInstance().logDirectionDownwards() ||
                    log_colors != Options::getInstance().colorLogEnabled() ||
                    log_newline != Options::getInstance().logNewline() ||
                    log_margin != Options::getInstance().logMargin() ||
                    log_timestamp != Options::getInstance().logTimestampEnabled() ||
                    log_timestamp_format != Options::getInstance().logTimestampFormat();
  log_goes_downwards = Options::getInstance().logDirectionDownwards();
  log_colors = Options::getInstance().colorLogEnabled();
  log_newline = Options::getInstance().logNewline();
  log_margin = Options::getInstance().logMargin();
  log_timestamp = Options::getInstance().logTimestampEnabled();
  log_timestamp_format = Options::getInstance().logTimestampFormat();
  if (regenerate) {
    regenerate_ic_chatlog();
  }

  set_size_and_pos(ui_ic_chatlog, "ic_chatlog");
  m_ic_log_rect = ui_ic_chatlog->geometry();
  ui_ic_chatlog->setFrameShape(QFrame::NoFrame);
  ui_ic_chatlog->setPlaceholderText(log_goes_downwards ? "▼ " + tr("Log goes down") + " ▼"
                                                       : "▲ " + tr("Log goes up") + " ▲");
  ui_find_bar->setLogGoesDown(log_goes_downwards);
  position_find_bar();

  set_size_and_pos(ui_debug_log, "ms_chatlog"); // Old name, still use it to not break compatibility
  ui_debug_log->setFrameShape(QFrame::NoFrame);

  set_size_and_pos(ui_server_chatlog, "server_chatlog");
  ui_server_chatlog->setFrameShape(QFrame::NoFrame);

  set_size_and_pos(ui_mute_list, "mute_list");
  ui_mute_list->hide();

  set_size_and_pos(ui_pair_list, "pair_list");
  ui_pair_list->hide();
  ui_pair_list->setToolTip(
      tr("Select a character you wish to pair with. Pairing works when you both "
         "choose each other and stand in the same position."));

  set_size_and_pos(ui_pair_offset_spinbox, "pair_offset_spinbox");
  ui_pair_offset_spinbox->hide();
  ui_pair_offset_spinbox->setToolTip(
      tr("Change the horizontal percentage offset of your character's position "
         "from the "
         "center of the screen."));

  set_size_and_pos(ui_pair_vert_offset_spinbox, "pair_vert_offset_spinbox");
  ui_pair_vert_offset_spinbox->hide();
  ui_pair_vert_offset_spinbox->setToolTip(
      tr("Change the vertical percentage offset of your character's position "
         "from the "
         "center of the screen."));

  ui_pair_order_dropdown->hide();
  set_size_and_pos(ui_pair_order_dropdown, "pair_order_dropdown");
  ui_pair_order_dropdown->setToolTip(
      tr("Where YOUR character stands when you speak: in front of your partners, "
         "behind them, or between two partners. Each player's setting applies to "
         "their own messages."));

  set_size_and_pos(ui_pair_button, "pair_button");
  ui_pair_button->set_image("pair_button");
  ui_pair_button->setToolTip(
      tr("Display the list of characters to pair with."));

  // Second partner controls, only on ULTA servers
  ui_pair_slot_dropdown->hide();
  sync_log_under_lists();
  if (ao_app->ulta_supported) {
    // "To middle" only makes sense with two partners
    if (ui_pair_order_dropdown->count() < 3)
      ui_pair_order_dropdown->addItem(tr("To middle"));

    ui_pair_slot_dropdown->setToolTip(
        tr("Choose which partner the character list below selects: "
           "partner 1 or partner 2."));
    pos_size_type slot_design = ao_app->get_element_dimensions(
        "pair_slot_dropdown", "courtroom_design.ini");
    if (slot_design.width >= 0 && slot_design.height >= 0) {
      // The theme positions it.
      set_size_and_pos(ui_pair_slot_dropdown, "pair_slot_dropdown");
    }
    else if (ui_pair_list->height() > 100) {
      // Fallback for themes that don't know about it: carve a row out of the
      // bottom of the pair list so nothing else in the layout is disturbed.
      int row_h = qMax(ui_pair_order_dropdown->height(), 24);
      ui_pair_slot_dropdown->setGeometry(
          ui_pair_list->x(), ui_pair_list->y() + ui_pair_list->height() - row_h,
          ui_pair_list->width(), row_h);
      ui_pair_list->resize(ui_pair_list->width(), ui_pair_list->height() - row_h);
    }
  }
  else {
    // Stock server: drop everything ULTA-specific from the pair UI.
    if (ui_pair_order_dropdown->count() > 2)
      ui_pair_order_dropdown->removeItem(2);
    pair_order = qMin(pair_order, 1);
    other_charid2 = -1;
    pair_slot = 0;
  }

  set_size_and_pos(ui_area_list, "music_list");
  ui_area_list->header()->setMinimumSectionSize(ui_area_list->width());

  set_size_and_pos(ui_music_list, "music_list");
  ui_music_list->header()->setMinimumSectionSize(ui_music_list->width());
  QString music_list_indentation = ao_app->get_design_element("music_list_indent", "courtroom_design.ini");
  if (music_list_indentation == "")
    ui_music_list->resetIndentation();
  else
    ui_music_list->setIndentation(music_list_indentation.toInt());

  QString music_list_animated = ao_app->get_design_element("music_list_animated", "courtroom_design.ini");
  ui_music_list->setAnimated(music_list_animated == "1" || music_list_animated.startsWith("true"));

  set_size_and_pos(ui_music_name, "music_name");

  ui_music_display->move(0, 0);
  pos_size_type design_ini_result =
      ao_app->get_element_dimensions("music_display", "courtroom_design.ini");

  if (design_ini_result.width < 0 || design_ini_result.height < 0) {
    qWarning() << "could not find \"music_display\" in courtroom_design.ini";
    ui_music_display->hide();
  }
  else {
    ui_music_display->move(design_ini_result.x, design_ini_result.y);
    ui_music_display->combo_resize(design_ini_result.width,
                                   design_ini_result.height);
  }
  ui_music_display->load_image("music_display", "");


  for (int i = 0; i < max_clocks; i++) {
    set_size_and_pos(ui_clock[i], "clock_" + QString::number(i));
    ui_clock[i]->raise(); // a theme may put the clocks over the viewport
  }
  set_size_and_pos(ui_ic_chat_message, "ao2_ic_chat_message");
  set_size_and_pos(ui_ic_chat_name, "ao2_ic_chat_name");

  initialize_chatbox();

  ui_vp_sticker->move(ui_viewport->x(), ui_viewport->y());
  ui_vp_sticker->combo_resize(ui_viewport->width(),
                              ui_viewport->height());

  ui_muted->resize(ui_ic_chat_message->width(), ui_ic_chat_message->height());
  ui_muted->set_image("muted");
  ui_muted->setToolTip(tr("Oops, you're muted!"));

  set_size_and_pos(ui_ooc_chat_message, "ooc_chat_message");;

  set_size_and_pos(ui_ooc_chat_name, "ooc_chat_name");

  // set_size_and_pos(ui_area_password, "area_password");
  set_size_and_pos(ui_music_search, "music_search");

  set_size_and_pos(ui_emote_dropdown, "emote_dropdown");
  ui_emote_dropdown->setToolTip(
      tr("Set your character's emote to play on your next message."));

  set_size_and_pos(ui_pos_dropdown, "pos_dropdown");
  ui_pos_dropdown->setEditable(true);
  ui_pos_dropdown->setInsertPolicy(QComboBox::NoInsert);
  ui_pos_dropdown->setToolTip(
      tr("Set your character's supplementary background."));

  set_size_and_pos(ui_pos_remove, "pos_remove");
  ui_pos_remove->setText("X");
  ui_pos_remove->set_image("evidencex");
  ui_pos_remove->setToolTip(tr("Reset your character's supplementary background to its default."));
  if (current_side == "")
    ui_pos_remove->hide();
  else
    ui_pos_remove->show();

  set_size_and_pos(ui_iniswap_dropdown, "iniswap_dropdown");
  ui_iniswap_dropdown->setEditable(true);
  ui_iniswap_dropdown->setInsertPolicy(QComboBox::InsertAtBottom);
  ui_iniswap_dropdown->setToolTip(
      tr("Set an 'iniswap', or an alternative character folder to refer to "
         "from your current character.\n"
         "Edit by typing and pressing Enter, [X] to remove. This saves to your "
         "base/iniswaps.ini"));

  set_size_and_pos(ui_iniswap_remove, "iniswap_remove");
  ui_iniswap_remove->setText("X");
  ui_iniswap_remove->set_image("evidencex");
  ui_iniswap_remove->setToolTip(
      tr("Remove the currently selected iniswap from the list and return to "
         "the original character folder."));
  ui_iniswap_remove->hide();

  set_size_and_pos(ui_sfx_dropdown, "sfx_dropdown");
  ui_sfx_dropdown->setEditable(true);
  ui_sfx_dropdown->setInsertPolicy(QComboBox::NoInsert);
  ui_sfx_dropdown->setToolTip(
      tr("Set a sound effect to play on your next 'Preanim'. Leaving it on "
         "Default will use the emote-defined sound (if any).\n"
         "Edit by typing and pressing Enter, [X] to remove. This saves to your "
         "base/characters/<charname>/soundlist.ini"));

  set_size_and_pos(ui_sfx_remove, "sfx_remove");
  ui_sfx_remove->setText("X");
  ui_sfx_remove->set_image("evidencex");
  ui_sfx_remove->setToolTip(tr("Remove the currently selected sound effect."));
  ui_sfx_remove->hide();

  set_iniswap_dropdown();

  set_size_and_pos(ui_effects_dropdown, "effects_dropdown");
  ui_effects_dropdown->setInsertPolicy(QComboBox::InsertAtBottom);
  ui_effects_dropdown->setToolTip(
      tr("Choose an effect to play on your next spoken message.\n"
         "The effects are defined in your theme/effects/effects.ini. Your "
         "character can define custom effects by\n"
         "char.ini [Options] category, effects = 'miscname' where it referes "
         "to misc/<miscname>/effects.ini to read the effects."));
  // Todo: recode this entire fucking system with these dumbass goddamn ini's
  // why is everything so specifically coded for all these purposes is ABSTRACT
  // CODING not a thing now huh what the FUCK why do I gotta do this pleASE FOR
  // THE LOVE OF GOD SPARE ME FROM THIS FRESH HELL btw i still love coding.
  QPoint p_point = ao_app->get_button_spacing("effects_icon_size", filename);
  ui_effects_dropdown->setIconSize(QSize(p_point.x(), p_point.y()));

  set_size_and_pos(ui_defense_bar, "defense_bar");
  ui_defense_bar->set_image("defensebar" + QString::number(defense_bar_state));

  set_size_and_pos(ui_prosecution_bar, "prosecution_bar");
  ui_prosecution_bar->set_image("prosecutionbar" +
                                QString::number(prosecution_bar_state));

  set_size_and_pos(ui_music_label, "music_label");
  ui_music_label->setText(tr("Music"));
  set_size_and_pos(ui_sfx_label, "sfx_label");
  ui_sfx_label->setText(tr("Sfx"));
  set_size_and_pos(ui_blip_label, "blip_label");
  ui_blip_label->setText(tr("Blips"));

  set_size_and_pos(ui_hold_it, "hold_it");
  ui_hold_it->setText(tr("Hold It!"));
  ui_hold_it->setToolTip(tr("When this is turned on, your next in-character "
                            "message will be a shout!"));
  ui_hold_it->set_image("holdit");

  set_size_and_pos(ui_objection, "objection");
  ui_objection->setText(tr("Objection!"));
  ui_objection->setToolTip(tr("When this is turned on, your next in-character "
                              "message will be a shout!"));
  ui_objection->set_image("objection");

  set_size_and_pos(ui_take_that, "take_that");
  ui_take_that->setText(tr("Take That!"));
  ui_take_that->setToolTip(tr("When this is turned on, your next in-character "
                              "message will be a shout!"));
  ui_take_that->set_image("takethat");

  set_size_and_pos(ui_ooc_toggle, "ooc_toggle");
  ui_ooc_toggle->setText(tr("Server"));
  ui_ooc_toggle->setToolTip(
      tr("Toggle between server chat and global AO2 chat."));

  set_size_and_pos(ui_witness_testimony, "witness_testimony");
  ui_witness_testimony->set_image("witnesstestimony");
  ui_witness_testimony->setToolTip(tr("This will display the animation in the "
                                      "viewport as soon as it is pressed."));
  set_size_and_pos(ui_cross_examination, "cross_examination");
  ui_cross_examination->set_image("crossexamination");
  ui_cross_examination->setToolTip(tr("This will display the animation in the "
                                      "viewport as soon as it is pressed."));

  set_size_and_pos(ui_guilty, "guilty");
  ui_guilty->setText(tr("Guilty!"));
  ui_guilty->set_image("guilty");
  ui_guilty->setToolTip(tr("This will display the animation in the viewport as "
                           "soon as it is pressed."));
  set_size_and_pos(ui_not_guilty, "not_guilty");
  ui_not_guilty->set_image("notguilty");
  ui_not_guilty->setToolTip(tr("This will display the animation in the "
                               "viewport as soon as it is pressed."));

  set_size_and_pos(ui_change_character, "change_character");
  ui_change_character->setText(tr("Change character"));
  ui_change_character->set_image("change_character");
  ui_change_character->setToolTip(
      tr("Bring up the Character Select Screen and change your character."));

  set_size_and_pos(ui_reload_theme, "reload_theme");
  ui_reload_theme->setText(tr("Reload theme"));
  ui_reload_theme->set_image("reload_theme");
  ui_reload_theme->setToolTip(
      tr("Refresh the theme and update all of the ui elements to match."));

  set_size_and_pos(ui_call_mod, "call_mod");
  ui_call_mod->setText(tr("Call mod"));
  ui_call_mod->set_image("call_mod");
  ui_call_mod->setToolTip(
      tr("Request the attention of the current server's moderator."));

  set_size_and_pos(ui_settings, "settings");
  ui_settings->setText(tr("Settings"));
  ui_settings->set_image("courtroom_settings");
  if (ui_settings->icon().isNull()) {
      ui_settings->set_image("settings"); // pre-2.10 filename
  }
  ui_settings->setToolTip(
      tr("Allows you to change various aspects of the client."));

  set_size_and_pos(ui_switch_area_music, "switch_area_music");
  ui_switch_area_music->setText(tr("A/M"));
  ui_switch_area_music->set_image("switch_area_music");
  ui_switch_area_music->setToolTip(tr("Switch between Areas and Music lists"));

  set_size_and_pos(ui_pre, "pre");
  ui_pre->setText(tr("Preanim"));
  ui_pre->setToolTip(
      tr("Play a single-shot animation as defined by the emote when checked."));

  ui_immediate->setToolTip(
      tr("If preanim is checked, display the input text immediately as the "
         "animation plays concurrently."));

  design_ini_result =
      ao_app->get_element_dimensions("immediate", "courtroom_design.ini");

  // If we don't have new-style naming, fall back to the old method
  if (design_ini_result.width < 0 || design_ini_result.height < 0)  {
    set_size_and_pos(ui_immediate, "pre_no_interrupt");
    truncate_label_text(ui_immediate, "pre_no_interrupt");
  }
  else {// Adopt the based new method instead
    set_size_and_pos(ui_immediate, "immediate");
    truncate_label_text(ui_immediate, "immediate");
  }


  set_size_and_pos(ui_flip, "flip");
  ui_flip->setToolTip(tr("Mirror your character's emotes when checked."));

  set_size_and_pos(ui_additive, "additive");
  ui_additive->setToolTip(
      tr("Add text to your last spoken message when checked."));

  set_size_and_pos(ui_guard, "guard");
  ui_guard->setToolTip(
      tr("Do not listen to mod calls when checked, preventing them from "
         "playing sounds or focusing attention on the window."));

  set_size_and_pos(ui_showname_enable, "showname_enable");
  ui_showname_enable->setToolTip(
      tr("Display customized shownames for all users when checked."));

  set_size_and_pos(ui_custom_objection, "custom_objection");
  ui_custom_objection->setText(tr("Custom Shout!"));
  ui_custom_objection->set_image("custom");
  ui_custom_objection->setToolTip(
      tr("This will display the custom character-defined animation in the "
         "viewport as soon as it is pressed.\n"
         "To make one, your character's folder must contain "
         "custom.[webp/apng/gif/png] and custom.[wav/ogg/opus] sound effect"));

  set_size_and_pos(ui_realization, "realization");
  ui_realization->set_image("realization");
  ui_realization->setToolTip(
      tr("Play realization sound and animation in the viewport on the next "
         "spoken message when checked."));

  set_size_and_pos(ui_screenshake, "screenshake");
  ui_screenshake->set_image("screenshake");
  ui_screenshake->setToolTip(
      tr("Shake the screen on next spoken message when checked."));

  set_size_and_pos(ui_mute, "mute_button");
  ui_mute->setText("Mute");
  ui_mute->set_image("mute");
  ui_mute->setToolTip(
      tr("Display the list of character folders you wish to mute."));

  set_size_and_pos(ui_defense_plus, "defense_plus");
  ui_defense_plus->set_image("defplus");
  ui_defense_plus->setToolTip(tr("Increase the health bar."));

  set_size_and_pos(ui_defense_minus, "defense_minus");
  ui_defense_minus->set_image("defminus");
  ui_defense_minus->setToolTip(tr("Decrease the health bar."));

  set_size_and_pos(ui_prosecution_plus, "prosecution_plus");
  ui_prosecution_plus->set_image("proplus");
  ui_prosecution_plus->setToolTip(tr("Increase the health bar."));

  set_size_and_pos(ui_prosecution_minus, "prosecution_minus");
  ui_prosecution_minus->set_image("prominus");
  ui_prosecution_minus->setToolTip(tr("Decrease the health bar."));

  set_size_and_pos(ui_text_color, "text_color");
  ui_text_color->setToolTip(
      tr("Change the text color of the spoken message.\n"
         "You can also select a part of your currently typed message and use "
         "the dropdown to change its color!"));
  set_text_color_dropdown();

  // Custom colors button (theme key `ulta_colors_button`), else a square off the right end of the text
  // color dropdown
  ui_ulta_colors->hide();
  if (ao_app->ulta_supported) {
    pos_size_type colors_design = ao_app->get_element_dimensions(
        "ulta_colors_button", "courtroom_design.ini");
    if (colors_design.width >= 0 && colors_design.height >= 0) {
      set_size_and_pos(ui_ulta_colors, "ulta_colors_button");
      ui_ulta_colors->show();
    }
    else if (ui_text_color->width() > 80) {
      int side = qMin(ui_text_color->height(), 28);
      ui_ulta_colors->setGeometry(
          ui_text_color->x() + ui_text_color->width() - side,
          ui_text_color->y(), side, ui_text_color->height());
      ui_text_color->resize(ui_text_color->width() - side,
                            ui_text_color->height());
      ui_ulta_colors->show();
    }
  }

  set_size_and_pos(ui_music_slider, "music_slider");
  set_size_and_pos(ui_sfx_slider, "sfx_slider");
  set_size_and_pos(ui_blip_slider, "blip_slider");

  set_size_and_pos(ui_back_to_lobby, "back_to_lobby");
  ui_back_to_lobby->setText(tr("Back to Lobby"));
  ui_back_to_lobby->setToolTip(tr("Return back to the server list."));

  set_size_and_pos(ui_char_password, "char_password");

  set_size_and_pos(ui_char_buttons, "char_buttons");

  set_size_and_pos(ui_char_select_left, "char_select_left");
  ui_char_select_left->set_image("arrow_left");

  set_size_and_pos(ui_char_select_right, "char_select_right");
  ui_char_select_right->set_image("arrow_right");

  set_size_and_pos(ui_spectator, "spectator");
  ui_spectator->setToolTip(tr("Become a spectator. You won't be able to "
                              "interact with the in-character screen."));

  // QCheckBox
  truncate_label_text(ui_guard, "guard");
  truncate_label_text(ui_pre, "pre");
  truncate_label_text(ui_flip, "flip");
  truncate_label_text(ui_showname_enable, "showname_enable");

  // QLabel
  truncate_label_text(ui_music_label, "music_label");
  truncate_label_text(ui_sfx_label, "sfx_label");
  truncate_label_text(ui_blip_label, "blip_label");

  free_brush =
      QBrush(ao_app->get_color("area_free_color", "courtroom_design.ini"));
  lfp_brush =
      QBrush(ao_app->get_color("area_lfp_color", "courtroom_design.ini"));
  casing_brush =
      QBrush(ao_app->get_color("area_casing_color", "courtroom_design.ini"));
  recess_brush =
      QBrush(ao_app->get_color("area_recess_color", "courtroom_design.ini"));
  rp_brush = QBrush(ao_app->get_color("area_rp_color", "courtroom_design.ini"));
  gaming_brush =
      QBrush(ao_app->get_color("area_gaming_color", "courtroom_design.ini"));
  locked_brush =
      QBrush(ao_app->get_color("area_locked_color", "courtroom_design.ini"));

  // Narrator button (theme key `ulta_narrator_button`), else a square off the right end of the showname box
  ui_ulta_narrator->hide();
  m_ulta_nr_placed = false;
  m_nr_side = 0;
  m_nr_name_shrunk = false;
  if (ao_app->ulta_supported) {
    pos_size_type narrator_design = ao_app->get_element_dimensions(
        "ulta_narrator_button", "courtroom_design.ini");
    if (narrator_design.width >= 0 && narrator_design.height >= 0) {
      set_size_and_pos(ui_ulta_narrator, "ulta_narrator_button");
      m_ulta_nr_placed = true;
    }
    else if (ui_ic_chat_name->width() > 50) {
      int side = qMin(ui_ic_chat_name->height(), 28);
      ui_ulta_narrator->setGeometry(
          ui_ic_chat_name->x() + ui_ic_chat_name->width() - side,
          ui_ic_chat_name->y(), side, ui_ic_chat_name->height());
      ui_ic_chat_name->resize(ui_ic_chat_name->width() - side,
                              ui_ic_chat_name->height());
      m_ulta_nr_placed = true;
      m_nr_side = side;
      m_nr_name_shrunk = true;
    }

    // Fade button (theme key `ulta_fade_button`), else floating over the lower right of the viewport
    ui_ulta_charfade->hide();
    pos_size_type fade_design = ao_app->get_element_dimensions(
        "ulta_fade_button", "courtroom_design.ini");
    if (fade_design.width >= 0 && fade_design.height >= 0) {
      set_size_and_pos(ui_ulta_charfade, "ulta_fade_button");
      ui_ulta_charfade->show();
    }
    else {
      // The showname box is too small in most themes to carve a second
      // button from, so it floats over the lower-right corner of the viewport.
      const int w = 30, h = 24;
      ui_ulta_charfade->setGeometry(
          ui_viewport->x() + ui_viewport->width() - w - 6,
          ui_viewport->y() + ui_viewport->height() - h - 6, w, h);
      ui_ulta_charfade->show();
      ui_ulta_charfade->raise();
    }
    update_charfade_button();

    // Flashback button (`ulta_flashback_button`), else next to the fade button. Panel:
    // `ulta_flashback_panel`, else the upper right of the viewport
    ui_ulta_flash->hide();
    pos_size_type flash_design = ao_app->get_element_dimensions(
        "ulta_flashback_button", "courtroom_design.ini");
    if (flash_design.width >= 0 && flash_design.height >= 0) {
      set_size_and_pos(ui_ulta_flash, "ulta_flashback_button");
    }
    else {
      const int w = 30, h = 24;
      ui_ulta_flash->setGeometry(
          ui_viewport->x() + ui_viewport->width() - 2 * w - 10,
          ui_viewport->y() + ui_viewport->height() - h - 6, w, h);
    }
    ui_ulta_flash->show();
    ui_ulta_flash->raise();

    pos_size_type flash_panel_design = ao_app->get_element_dimensions(
        "ulta_flashback_panel", "courtroom_design.ini");
    if (flash_panel_design.width >= 0 && flash_panel_design.height >= 0) {
      ui_flash_panel->setGeometry(flash_panel_design.x, flash_panel_design.y,
                                  flash_panel_design.width,
                                  flash_panel_design.height);
    }
    else {
      ui_flash_panel->adjustSize();
      const QPoint default_pos(
          ui_viewport->x() + ui_viewport->width() - ui_flash_panel->width() - 6,
          ui_viewport->y() + 6);
      ui_flash_panel->move(m_flash_panel_has_pos ? m_flash_panel_pos
                                                 : default_pos);
    }
    ui_flash_panel->clampToParent();
    sync_flash_panel();

    // Status strip (`ulta_status_strip`), else a thin bar along the top of the viewport
    pos_size_type status_design = ao_app->get_element_dimensions(
        "ulta_status_strip", "courtroom_design.ini");
    if (status_design.width >= 0 && status_design.height >= 0) {
      set_size_and_pos(ui_ulta_status, "ulta_status_strip");
      ui_ulta_status->setStyleSheet("");
    }
    else {
      ui_ulta_status->setGeometry(ui_viewport->x(), ui_viewport->y(),
                                  ui_viewport->width(), 18);
      ui_ulta_status->setStyleSheet(
          "QLabel#ui_ulta_status { background-color: rgba(0, 0, 0, 150); "
          "color: #e4e6eb; padding-left: 6px; }");
    }
    ui_ulta_status->raise();
    update_ulta_status();

    // Narrator panel (`ulta_narrator_panel`), else floating over the lower left of the viewport
    pos_size_type panel_design = ao_app->get_element_dimensions(
        "ulta_narrator_panel", "courtroom_design.ini");
    if (panel_design.width >= 0 && panel_design.height >= 0) {
      ui_narrator_panel->setGeometry(panel_design.x, panel_design.y,
                                     panel_design.width, panel_design.height);
    }
    else {
      const int panel_w = 270, panel_h = 112;
      // Where the user last dragged it, else the default corner.
      const QPoint default_pos(
          ui_viewport->x() + 6,
          ui_viewport->y() + ui_viewport->height() - panel_h - 6);
      const QPoint pos =
          ulta_narrator_panel_has_pos ? ulta_narrator_panel_pos : default_pos;
      Q_UNUSED(panel_w);
      ui_narrator_panel->move(pos);
      ui_narrator_panel->adjustSize();
    }
    ui_narrator_panel->clampToParent();
    update_narrator_button();
  }

  refresh_evidence();
}

void Courtroom::set_fonts(QString p_char)
{
  set_font(ui_vp_showname, "", "showname", p_char);
  set_font(ui_vp_message, "", "message", p_char);
  set_font(ui_ic_chatlog, "", "ic_chatlog", p_char);
  set_font(ui_debug_log, "", "debug_log", p_char);
  set_font(ui_server_chatlog, "", "server_chatlog", p_char);
  set_font(ui_music_list, "", "music_list", p_char);
  set_font(ui_area_list, "", "area_list", p_char);
  set_font(ui_music_name, "", "music_name", p_char);

  for (int i = 0; i < max_clocks; i++)
    set_font(ui_clock[i], "", "clock_" + QString::number(i), p_char);

  set_stylesheets();
}

void Courtroom::set_font(QWidget *widget, QString class_name,
                         QString p_identifier, QString p_char,
                         QString font_name, int f_pointsize)
{
  QString design_file = "courtroom_fonts.ini";
  if (f_pointsize <= 0)
    f_pointsize =
        ao_app->get_design_element(p_identifier, design_file, ao_app->get_chat(p_char)).toInt();
  if (font_name == "")
    font_name =
        ao_app->get_design_element(p_identifier + "_font", design_file, ao_app->get_chat(p_char));
  QString f_color_result =
      ao_app->get_design_element(p_identifier + "_color", design_file, ao_app->get_chat(p_char));
  QColor f_color(0, 0, 0);
  if (f_color_result != "") {
    QStringList color_list = f_color_result.split(",");

    if (color_list.size() >= 3) {
      f_color.setRed(color_list.at(0).toInt());
      f_color.setGreen(color_list.at(1).toInt());
      f_color.setBlue(color_list.at(2).toInt());
    }
  }
  bool bold =
      ao_app->get_design_element(p_identifier + "_bold", design_file, ao_app->get_chat(p_char)) ==
      "1"; // is the font bold or not?
  bool antialias = ao_app->get_design_element(p_identifier + "_sharp",
                                              design_file, ao_app->get_chat(p_char)) !=
                   "1"; // is the font anti-aliased or not?

  this->set_qfont(widget, class_name,
                  get_qfont(font_name, f_pointsize, antialias), f_color, bold);
}

QFont Courtroom::get_qfont(QString font_name, int f_pointsize, bool antialias)
{
  QFont font;
  if (font_name.isEmpty())
    font_name = "Arial";

  QFont::StyleStrategy style_strategy = QFont::PreferDefault;
  if (!antialias)
    style_strategy = QFont::NoAntialias;

#ifdef Q_OS_MAC
  // Points are smaller on macOS (72 dpi base)
  f_pointsize = qRound(f_pointsize * 4.0 / 3.0);
#endif
  font = QFont(font_name, f_pointsize);
  font.setStyleHint(QFont::SansSerif, style_strategy);
  return font;
}

void Courtroom::set_qfont(QWidget *widget, QString class_name, QFont font,
                          QColor f_color, bool bold)
{
  if (class_name.isEmpty())
    class_name = widget->metaObject()->className();

  font.setBold(bold);
  widget->setFont(font);

  QString style_sheet_string =
      class_name + " { color: rgba(" +
      QString::number(f_color.red()) + ", " + QString::number(f_color.green()) +
      ", " + QString::number(f_color.blue()) + ", 255);}";
  widget->setStyleSheet(style_sheet_string);
}

void Courtroom::set_stylesheet(QWidget *widget)
{
  QString f_file = "courtroom_stylesheets.css";
  QString style_sheet_string = ao_app->get_stylesheet(f_file);
  if (style_sheet_string != "")
    widget->setStyleSheet(style_sheet_string);
}

void Courtroom::set_stylesheets()
{
  set_stylesheet(this);
  this->setStyleSheet(
    "QFrame { background-color:transparent; } "
    "QAbstractItemView { background-color: transparent; color: black; } "
    "QLineEdit { background-color:transparent; }"
    + this->styleSheet()
  );
}

void Courtroom::set_window_title(QString p_title)
{
  this->setWindowTitle(p_title);
}

void Courtroom::set_size_and_pos(QWidget *p_widget, QString p_identifier, QString p_misc)
{
  QString filename = "courtroom_design.ini";

  pos_size_type design_ini_result =
      ao_app->get_element_dimensions(p_identifier, filename, p_misc);

  if (design_ini_result.width < 0 || design_ini_result.height < 0) {
    qWarning() << "could not find" << p_identifier << "in" << filename;
    p_widget->hide();
  }
  else {
    p_widget->move(design_ini_result.x, design_ini_result.y);
    p_widget->resize(design_ini_result.width, design_ini_result.height);
  }
}

void Courtroom::set_taken(int n_char, bool p_taken)
{
  if (n_char >= char_list.size()) {
    qWarning() << "set_taken attempted to set an index bigger than char_list size";
    return;
  }

  char_type f_char;
  f_char.name = char_list.at(n_char).name;
  f_char.description = char_list.at(n_char).description;
  f_char.taken = p_taken;
  f_char.evidence_string = char_list.at(n_char).evidence_string;

  char_list.replace(n_char, f_char);
}

QPoint Courtroom::get_theme_pos(QString p_identifier)
{
  QString filename = "courtroom_design.ini";

  pos_size_type design_ini_result =
      ao_app->get_element_dimensions(p_identifier, filename);

  if (design_ini_result.width < 0 || design_ini_result.height < 0) {
    qWarning() << "could not find" << p_identifier << "in" << filename;
    return QPoint(0, 0);
  }
  else {
    return QPoint(design_ini_result.x, design_ini_result.y);
  }
}

void Courtroom::done_received()
{
  m_cid = -1;

  if (char_list.size() > 0)
  {
    set_char_select();
  }
  else
  {
    update_character(m_cid);
    enter_courtroom();
    set_courtroom_size();
  }

  set_mute_list();
  set_pair_list();

  show();

  ui_spectator->show();
}

void Courtroom::set_background(QString p_background, bool display)
{
  // A new area starts with a clear scene (the BN with a position). A background change inside the area
  // keeps a blackout, so "go dark, swap the background, fade in" works
  if (display) {
    clear_scene_fx();
    clear_tool_overlay();
    clear_flashback_local();
    m_charfx_gone.clear();
    m_charfx_away.clear();
    update_charfade_button();
  }
  ui_vp_testimony->stop();
  current_background = p_background;

  // welcome to hardcode central may I take your order of regularly scheduled
  // CBT
  QMap<QString, QString> default_pos;
  default_pos["defenseempty"] = "def";
  default_pos["helperstand"] = "hld";
  default_pos["prosecutorempty"] = "pro";
  default_pos["prohelperstand"] = "hlp";
  default_pos["witnessempty"] = "wit";
  default_pos["judgestand"] = "jud";
  default_pos["jurystand"] = "jur";
  default_pos["seancestand"] = "sea";

  // Populate the dropdown list with all pos that exist on this bg
  QStringList pos_list = {};
  for (const QString &key : default_pos.keys()) {
    if (file_exists(ao_app->get_image_suffix(
            ao_app->get_background_path(default_pos[key]))) || // if we have 2.8-style positions, e.g. def.png, wit.webp, hld.apng
        file_exists(
            ao_app->get_image_suffix(ao_app->get_background_path(key)))) { // if we have pre-2.8-style positions, e.g. defenseempty.png
      pos_list.append(default_pos[key]);
    }
  }
  for (const QString &pos : ao_app->read_design_ini("positions", ao_app->get_background_path("design.ini")).split(",")) {
    if (file_exists(ao_app->get_image_suffix(ao_app->get_background_path(pos)))) {
      pos_list.append(pos);
    }
  }

  set_pos_dropdown(pos_list);

  if (display) {
    ui_vp_speedlines->hide();
    ui_vp_player_char->stop();

    ui_vp_sideplayer_char->stop();
    ui_vp_sideplayer2_char->stop();
    ui_vp_effect->stop();
    ui_vp_message->hide();
    ui_vp_chatbox->setVisible(chatbox_always_show);
    // Show it if chatbox always shows
    if (Options::getInstance().characterStickerEnabled() && chatbox_always_show) {
      ui_vp_sticker->load_image(m_chatmessage[CHAR_NAME]);
    }
    // Hide the face sticker
    else {
      ui_vp_sticker->stop();
    }
    // Stop the chat arrow from animating
    ui_vp_chat_arrow->hide();

    // Clear the message queue
    text_queue_timer->stop();
    skip_chatmessage_queue();

    text_state = 2;
    anim_state = 3;
    ui_vp_objection->stop();
    chat_tick_timer->stop();
    ui_vp_evidence_display->reset();
    QString f_side = current_side;
    if (current_side == "")
      f_side = ao_app->get_char_side(current_char);
    set_scene(true, f_side);
  }
}

void Courtroom::set_side(QString p_side)
{
  if (p_side.isEmpty() || p_side == ao_app->get_char_side(current_char)) {
      ui_pos_remove->hide();
      current_side = ao_app->get_char_side(current_char);
  }
  else {
      ui_pos_remove->show();
      current_side = p_side;
  }

  set_judge_buttons();

  // Block the signals to prevent setCurrentIndex from triggering a pos
  // change
  ui_pos_dropdown->blockSignals(true);
  for (int i = 0; i < ui_pos_dropdown->count(); ++i) {
    QString pos = ui_pos_dropdown->itemText(i);
    if (pos == current_side) {

      // Set the index on dropdown ui element to let you know what pos you're on
      // right now
      ui_pos_dropdown->setCurrentIndex(i);
      // Unblock the signals so the element can be used for setting pos again
      ui_pos_dropdown->blockSignals(false);

      // alright we dun, jobs done here boyos
      return;
    }
  }
  // We will only get there if we failed the last step
  ui_pos_dropdown->setEditText(current_side);
  // Unblock the signals so the element can be used for setting pos again
  ui_pos_dropdown->blockSignals(false);
}

void Courtroom::set_pos_dropdown(QStringList pos_dropdowns)
{
  // Block the signals to prevent setCurrentIndex from triggering a pos change
  ui_pos_dropdown->blockSignals(true);
  pos_dropdown_list = pos_dropdowns;
  ui_pos_dropdown->clear();
  for (int n = 0; n < pos_dropdown_list.size(); ++n) {
    QString pos = pos_dropdown_list.at(n);
    ui_pos_dropdown->addItem(pos);
    QPixmap image = QPixmap(ao_app->get_image_suffix(ao_app->get_background_path(ao_app->get_pos_path(pos))));
    if (!image.isNull()) {
      image = image.scaledToHeight(ui_pos_dropdown->iconSize().height());
    }
    ui_pos_dropdown->setItemIcon(n, image);
  }

  if (current_side != "" && !pos_dropdown_list.contains(current_side))
    ui_pos_dropdown->setEditText(current_side);

  // Unblock the signals so the element can be used for setting pos again
  ui_pos_dropdown->blockSignals(false);
}

void Courtroom::update_character(int p_cid, QString char_name, bool reset_emote)
{
  bool newchar = m_cid != p_cid;

  m_cid = p_cid;

  QString f_char;

  if (m_cid == -1) {
    if (Options::getInstance().discordEnabled())
      ao_app->discord->state_spectate();
    f_char = "";
  }
  else {
    f_char = char_name;
    if (char_name.isEmpty()) {
      f_char = char_list.at(m_cid).name;
    }

    if (Options::getInstance().discordEnabled())
      ao_app->discord->state_character(f_char.toStdString());
  }

  current_char = f_char;
  current_side = ao_app->get_char_side(current_char);
  set_side(current_side);

  set_text_color_dropdown();

  // If our cid changed or we're being told to reset
  if (newchar || reset_emote) {
    current_emote_page = 0;
    current_emote = 0;
  }

  if (m_cid == -1)
    ui_emotes->hide();
  else
    ui_emotes->show();

  refresh_emotes();
  set_emote_page();
  set_emote_dropdown();

  set_sfx_dropdown();
  set_effects_dropdown();

  if (newchar) // Avoid infinite loop of death and suffering
    set_iniswap_dropdown();

  ui_custom_objection->hide();
  if (ao_app->custom_objection_supported) // if setting is enabled
  {
    custom_obj_menu->clear();
    custom_objections_list.clear();
    if (file_exists(ao_app->get_image_suffix(
            ao_app->get_character_path(current_char, "custom")))) {
      ui_custom_objection->show();
      QString custom_name = ao_app->read_char_ini(f_char, "custom_name", "Shouts");
      QAction *action;
      if (custom_name != "")
        action = custom_obj_menu->addAction(custom_name);
      else
        action = custom_obj_menu->addAction("Default");
      custom_obj_menu->setDefaultAction(action);
      objection_custom = "";
    }
    QString custom_objection_dir = ao_app->get_real_path(
          ao_app->get_character_path(current_char, "custom_objections"));
    if (dir_exists(custom_objection_dir)) {
      ui_custom_objection->show();
      QDir directory(custom_objection_dir);
      QStringList custom_obj = directory.entryList(QStringList() << "*.png"
                                                                 << "*.gif"
                                                                 << "*.apng"
                                                                 << "*.webp",
                                                   QDir::Files);
      for (const QString &filename : custom_obj) {
        CustomObjection custom_objection;
        custom_objection.filename = filename;
        QString custom_name = ao_app->read_char_ini(f_char, filename.left(filename.lastIndexOf(".")) + "_name", "Shouts");
        QAction *action;
        if (custom_name != "") {
          custom_objection.name = custom_name;
          action = custom_obj_menu->addAction(custom_name);
        }
        else {
          custom_objection.name = filename.left(filename.lastIndexOf("."));
          action = custom_obj_menu->addAction(custom_objection.name);
        }
        if (custom_obj_menu->defaultAction() == nullptr) {
          custom_obj_menu->setDefaultAction(action);
          objection_custom = custom_objection.filename;
        }
      custom_objections_list.append(custom_objection);
      }
    }
  }

  if (m_cid != -1) {
    ui_ic_chat_name->setPlaceholderText(char_list.at(m_cid).name);
  }
  else {
    ui_ic_chat_name->setPlaceholderText("Spectator");
  }
  ui_char_select_background->hide();
  sync_narrator_panel(); // ULTA: the narrator panel returns with the scene
  ui_ic_chat_message->setEnabled(m_cid != -1);
  ui_ic_chat_message->setFocus();
  update_audio_volume();
}

void Courtroom::enter_courtroom()
{
  set_evidence_page();

  if (ao_app->flipping_supported)
    ui_flip->show();
  else
    ui_flip->hide();

  if (ao_app->additive_text_supported)
    ui_additive->show();
  else
    ui_additive->hide();

  list_music();
  list_areas();

  switch (
      objection_state) // no need to reset these as it was done in set_widgets()
  {
  case 1:
    ui_hold_it->set_image("holdit_selected");
    break;
  case 2:
    ui_objection->set_image("objection_selected");
    break;
  case 3:
    ui_take_that->set_image("takethat_selected");
    break;
  case 4:
    ui_custom_objection->set_image("custom_selected");
    break;
  default:
    break;
  }

  // Unmute everything
  music_player->set_muted(false);
  objection_player->set_muted(false);
  sfx_player->set_muted(false);
  blip_player->set_muted(false);

  // Update the audio sliders
  update_audio_volume();

  ui_vp_testimony->stop();
  // ui_server_chatlog->setHtml(ui_server_chatlog->toHtml());
}

// Todo: multithread this due to some servers having large as hell music list
// Colors one song row: the playing song gets its own color, the others the
// found/missing colors.
void Courtroom::apply_song_colors(QTreeWidgetItem *item)
{
  const QString file = "courtroom_design.ini";
  const QString song = item->text(1);
  if (song.isEmpty() || item->text(0) == song) // a category, not a song
    return;
  if (song == m_playing_song) {
    QColor color(45, 110, 200); // default for themes that don't set one
    const QString value = ao_app->get_design_element("playing_song_color", file);
    if (!value.isEmpty()) {
      const QStringList rgb = value.split(",");
      if (rgb.size() >= 3)
        color = QColor(rgb.at(0).toInt(), rgb.at(1).toInt(), rgb.at(2).toInt());
    }
    item->setBackground(0, QBrush(color));
    m_playing_item = item;
    m_playing_color = color;
    const QString pulse = ao_app->get_design_element("playing_song_pulse", file);
    if (pulse == "1") {
      if (!m_song_pulse) {
        m_song_pulse = new QTimer(this);
        m_song_pulse->setInterval(80);
        connect(m_song_pulse, &QTimer::timeout, this, &Courtroom::pulse_playing_song);
      }
      if (!m_song_pulse->isActive())
        m_song_pulse->start();
    }
    const QString text_value =
        ao_app->get_design_element("playing_song_text_color", file);
    const QStringList text_rgb = text_value.split(",");
    if (text_rgb.size() >= 3)
      item->setForeground(0, QBrush(QColor(text_rgb.at(0).toInt(), text_rgb.at(1).toInt(),
                                           text_rgb.at(2).toInt())));
    return;
  }
  if (item == m_playing_item)
    m_playing_item = nullptr;
  item->setForeground(0, QBrush()); // back to the list's default text color
  const QString path = ao_app->get_real_path(ao_app->get_music_path(song));
  item->setBackground(0, QBrush(ao_app->get_color(
                             file_exists(path) ? "found_song_color" : "missing_song_color", file)));
}

void Courtroom::update_playing_song_highlight()
{
  QTreeWidgetItemIterator it(ui_music_list);
  while (*it) {
    apply_song_colors(*it);
    ++it;
  }
  if (!m_playing_item && m_song_pulse)
    m_song_pulse->stop(); // nothing is playing (or it is not in this list)
}

void Courtroom::pulse_playing_song()
{
  // Only work while the music list can be seen.
  if (!m_playing_item || !ui_music_list->isVisible())
    return;
  m_pulse_phase += 0.30; // about 1.7 seconds per cycle at 80 ms ticks
  const double t = (std::sin(m_pulse_phase) + 1.0) / 2.0;
  const double f = 0.84 + 0.16 * t; // between 84% and 100% of the base color
  m_playing_item->setBackground(
      0, QBrush(QColor(int(m_playing_color.red() * f),
                       int(m_playing_color.green() * f),
                       int(m_playing_color.blue() * f))));
}

void Courtroom::list_music()
{
  m_playing_item = nullptr; // the old rows are about to be deleted
  ui_music_list->clear();
  //  ui_music_search->setText("");

  QString f_file = "courtroom_design.ini";

  QBrush found_brush(ao_app->get_color("found_song_color", f_file));
  QBrush missing_brush(ao_app->get_color("missing_song_color", f_file));

  int n_listed_songs = 0;

  QTreeWidgetItem *parent = nullptr;
  for (int n_song = 0; n_song < music_list.size(); ++n_song) {
    QString i_song = music_list.at(n_song);
    // It's a stop song or a stop category
    // yes we cannot properly parse a stop song without a stop category cuz otherwise areas break
    // I hate this program
    // I hate qt5 for making .remove and .replace return a reference to the string (modify original var)
    // instead of returning a new string
    // please end my suffering
    QString temp = i_song;
    if (i_song == "~stop.mp3" || (temp.remove('=').toLower() == "stop"))
        continue;
    QString i_song_listname = i_song.left(i_song.lastIndexOf("."));
    i_song_listname = i_song_listname.right(
        i_song_listname.length() - (i_song_listname.lastIndexOf("/") + 1));
    QTreeWidgetItem *treeItem;
    if (i_song_listname != i_song &&
        parent != nullptr) // not a category, parent exists
      treeItem = new QTreeWidgetItem(parent);
    else
      treeItem = new QTreeWidgetItem(ui_music_list);
    treeItem->setText(0, i_song_listname);
    treeItem->setText(1, i_song);

    QString song_path = ao_app->get_real_path(ao_app->get_music_path(i_song));

    if (file_exists(song_path))
      treeItem->setBackground(0, found_brush);
    else
      treeItem->setBackground(0, missing_brush);
    if (i_song_listname != i_song && i_song == m_playing_song)
      apply_song_colors(treeItem);

    if (i_song_listname ==
        i_song) // Not supposed to be a song to begin with - a category?
      parent = treeItem;
    ++n_listed_songs;
  }

  ui_music_list->expandAll(); // Needs to somehow remember which categories were
                              // expanded/collapsed if the music list didn't
                              // change since last time
  if (ui_music_search->text() != "") {
    on_music_search_edited(ui_music_search->text());
  }
}

// Todo: multithread this due to some servers having large as hell area list
void Courtroom::list_areas()
{
  int n_listed_areas = 0;

  for (int n_area = 0; n_area < area_list.size(); ++n_area) {
    QString i_area = "";
    i_area.append(area_list.at(n_area));

    if (ao_app->arup_supported) {
      i_area.append("\n  ");

      i_area.append(arup_statuses.at(n_area));

      if (arup_cms.at(n_area) != "FREE") {
        i_area.append(" | CM: ");
        i_area.append(arup_cms.at(n_area));
      }

      i_area.append("\n  ");

      if (arup_players.at(n_area) != -1) {
        i_area.append(QString::number(arup_players.at(n_area)));
        i_area.append(" users | ");
      }

      i_area.append(arup_locks.at(n_area));
    }


    QTreeWidgetItem *treeItem = ui_area_list->topLevelItem(n_area);
    if (treeItem == nullptr) {
      treeItem = new QTreeWidgetItem(ui_area_list);
    }
    treeItem->setText(0, area_list.at(n_area));
    treeItem->setText(1, i_area);

    if (ao_app->arup_supported) {
      // Coloring logic here.
      treeItem->setBackground(1, free_brush);
      if (arup_locks.at(n_area) == "LOCKED") {
        treeItem->setBackground(1, locked_brush);
      }
      else {
        if (arup_statuses.at(n_area) == "LOOKING-FOR-PLAYERS")
          treeItem->setBackground(1, lfp_brush);
        else if (arup_statuses.at(n_area) == "CASING")
          treeItem->setBackground(1, casing_brush);
        else if (arup_statuses.at(n_area) == "RECESS")
          treeItem->setBackground(1, recess_brush);
        else if (arup_statuses.at(n_area) == "RP")
          treeItem->setBackground(1, rp_brush);
        else if (arup_statuses.at(n_area) == "GAMING")
          treeItem->setBackground(1, gaming_brush);
      }
    }
    else {
      treeItem->setBackground(1, free_brush);
    }

    ++n_listed_areas;
  }

  while (ui_area_list->topLevelItemCount() > n_listed_areas) {
    ui_area_list->takeTopLevelItem(ui_area_list->topLevelItemCount()-1);
  }

  if (ui_music_search->text() != "") {
    on_music_search_edited(ui_music_search->text());
  }
}

void Courtroom::debug_message_handler(int type, const QString &msg)
{
  // Keep a bounded queue; drop the excess and say so.
  if (m_debug_pending.size() >= 300) {
    ++m_debug_dropped;
    return;
  }
  m_debug_pending.append(qMakePair(type, msg));
  if (!m_debug_flush->isActive())
    m_debug_flush->start(150);
}

void Courtroom::flush_debug_log()
{
  static const QHash<int, QString> names = {
    {QtDebugMsg, "debug"},
    {QtInfoMsg, "info"},
    {QtWarningMsg, "warn"},
    {QtCriticalMsg, "critical"},
    {QtFatalMsg, "fatal"}
  };
  // Write a limited batch per tick so the window stays responsive.
  int written = 0;
  while (!m_debug_pending.isEmpty() && written < 60) {
    const QPair<int, QString> entry = m_debug_pending.takeFirst();
    const QString name = names.value(entry.first, "info");
    if (!m_debug_colors.contains(entry.first)) {
      m_debug_colors.insert(
          entry.first,
          ao_app->get_color(QString("debug_log_%1_color").arg(name),
                            "courtroom_fonts.ini").name());
    }
    ui_debug_log->append_chatmessage(name, entry.second, QString(),
                                     m_debug_colors.value(entry.first));
    ++written;
  }
  if (m_debug_dropped > 0 && m_debug_pending.isEmpty()) {
    ui_debug_log->append_chatmessage(
        "info", tr("%1 messages skipped").arg(m_debug_dropped), QString(),
        m_debug_colors.value(QtInfoMsg, QString()));
    m_debug_dropped = 0;
  }
  if (!m_debug_pending.isEmpty())
    m_debug_flush->start(150);
}

void Courtroom::append_debug_message(QString f_message)
{
  ui_debug_log->append_chatmessage(
      QString(), f_message,
      ao_app->get_color("debug_log_color", "courtroom_fonts.ini").name());
}

void Courtroom::append_server_chatmessage(QString p_name, QString p_message,
                                          QString p_color)
{
  QString color = "#000000";

  if (p_color == "0")
    color = ao_app->get_color("ms_chatlog_sender_color", "courtroom_fonts.ini")
                .name();
  if (p_color == "1")
    color =
        ao_app->get_color("server_chatlog_sender_color", "courtroom_fonts.ini")
            .name();
  if (!ao_app->auth_packet_supported && p_message == "Logged in as a moderator.") {
    // Emulate successful authentication
    on_authentication_state_received(1);
  }

  ui_server_chatlog->append_chatmessage(p_name, p_message, color);

  if (Options::getInstance().logToTextFileEnabled() && !ao_app->log_filename.isEmpty()) {
    QString full = "[OOC][" + QDateTime::currentDateTimeUtc().toString() + "] " + p_name + ": " + p_message;
    ao_app->append_to_file(full, ao_app->log_filename, true);
  }
}

void Courtroom::on_authentication_state_received(int p_state)
{
  if (p_state >= 1) {
    ui_guard->show();
    append_server_chatmessage(tr("CLIENT"), tr("You were granted the Disable Modcalls button."), "1");
  }
  else if (p_state == 0) {
    append_server_chatmessage(tr("CLIENT"), tr("Login unsuccessful."), "1");
  }
  else if (p_state < 0) {
    ui_guard->hide();
    append_server_chatmessage(tr("CLIENT"), tr("You were logged out."), "1");
  }
}

void Courtroom::on_chat_return_pressed()
{
  if (is_muted)
    return;

  ui_ic_chat_message->blockSignals(true);
  QTimer::singleShot(Options::getInstance().chatRateLimit(), this,
                     [this] { ui_ic_chat_message->blockSignals(false); });

  // An armed scene tool plays now, with the typed line
  if (m_armed_tool != 0 && ulta_narrator_on && ao_app->ulta_supported &&
      ao_app->ulta_director) {
    const int tool = m_armed_tool;
    disarm_scene_tool();
    // Tools go out as a normal narrated message with the tool flag
    m_pending_fx_token = tool < 0 ? scene_fx_token(tool)
                                  : QString("t%1").arg(tool);
  }
  // MS#
  // deskmod#
  // pre-emote#
  // character#
  // emote#
  // message#
  // side#
  // sfx-name#
  // emote_modifier#
  // char_id#
  // sfx_delay#
  // objection_modifier#
  // evidence#
  // placeholder#
  // realization#
  // text_color#%

  // Additionally, in our case:

  // showname#
  // other_charid#
  // self_offset#
  // immediate_preanim#%

  QStringList packet_contents;
  QString f_side;

  if (current_side == "")
    f_side = ao_app->get_char_side(current_char);
  else
    f_side = current_side;

  int f_desk_mod = DESK_SHOW;

  if (ao_app->desk_mod_supported) {
    f_desk_mod = ao_app->get_desk_mod(current_char, current_emote);
    if (!ao_app->expanded_desk_mods_supported) {
      if (f_desk_mod == DESK_PRE_ONLY_EX || f_desk_mod == DESK_PRE_ONLY)
        f_desk_mod = DESK_HIDE;
      else if (f_desk_mod == DESK_EMOTE_ONLY_EX || f_desk_mod == DESK_EMOTE_ONLY)
        f_desk_mod = DESK_SHOW;
    }
    if (f_desk_mod == -1)
      f_desk_mod = DESK_SHOW;
  }

  packet_contents.append(QString::number(f_desk_mod));

  QString f_pre = ao_app->get_pre_emote(current_char, current_emote);
  int f_emote_mod = ao_app->get_emote_mod(current_char, current_emote);
  QString f_sfx = "1";
  int f_sfx_delay = get_char_sfx_delay();

// EMOTE MOD OVERRIDES:
  // Emote_mod 2 is only used by objection check later, having it in the char.ini does nothing
  if (f_emote_mod == 2) {
    f_emote_mod = PREANIM;
  }
  // No clue what emote_mod 3 is even supposed to be.
  if (f_emote_mod == 3) {
    f_emote_mod = IDLE;
  }
  // Emote_mod 4 seems to be a legacy bugfix that just refers it to emote_mod 5 which is zoom emote
  if (f_emote_mod == 4) {
    f_emote_mod = ZOOM;
  }
  // If we have "pre" on, and immediate is not checked
  if (ui_pre->isChecked() && !ui_immediate->isChecked()) {
    // Turn idle into preanim
    if (f_emote_mod == IDLE) {
      f_emote_mod = PREANIM;
    }
    // Turn zoom into preanim zoom
    else if (f_emote_mod == ZOOM && ao_app->prezoom_supported) {
      f_emote_mod = PREANIM_ZOOM;
    }
    // Play the sfx
    f_sfx = get_char_sfx();
  }
  // If we have "pre" off, or immediate is checked
  else {
    // Turn preanim into idle
    if (f_emote_mod == PREANIM) {
      f_emote_mod = IDLE;
    }
    // Turn preanim zoom into zoom
    else if (f_emote_mod == PREANIM_ZOOM) {
      f_emote_mod = ZOOM;
    }

    // Play the sfx if pre is checked
    if (ui_pre->isChecked()) {
      f_sfx = get_char_sfx();
    }
  }

  // Custom sfx override via sound list dropdown.
  if (!custom_sfx.isEmpty() || ui_sfx_dropdown->currentIndex() != 0) {
    f_sfx = get_char_sfx();
    // We have a custom sfx but we're on idle emotes.
    // Turn them into pre so the sound plays if client setting sfx_on_idle is enabled.
    if (Options::getInstance().playSelectedSFXOnIdle() && (f_emote_mod == IDLE || f_emote_mod == ZOOM)) {
      // We turn idle into preanim, but make it not send a pre animation
      f_pre = "";
      // Set sfx delay to 0 so the sfx plays immediately
      f_sfx_delay = 0;
      // Set the emote mod to preanim so the sound plays
      f_emote_mod = f_emote_mod == IDLE ? PREANIM : PREANIM_ZOOM;
    }
  }

  packet_contents.append(f_pre);

  packet_contents.append(current_char);

  packet_contents.append(ao_app->get_emote(current_char, current_emote));

  QString f_text = ui_ic_chat_message->text();
  if (!m_pending_fx_token.isEmpty() && f_text.trimmed().isEmpty())
    f_text = " ";
  packet_contents.append(f_text);

  packet_contents.append(f_side);

  packet_contents.append(f_sfx);
  packet_contents.append(QString::number(f_emote_mod));
  packet_contents.append(QString::number(m_cid));

  packet_contents.append(QString::number(f_sfx_delay));

  QString f_obj_state;

  if ((objection_state == 4 && !ao_app->custom_objection_supported) ||
      (objection_state < 0))
    f_obj_state = "0";
  else if (objection_custom != "" && objection_state == 4) {
    f_obj_state = QString::number(objection_state) + "&" +
                  objection_custom; // we add the name of the objection so the
                                    // packet is like: 4&(name of custom obj)
  }
  else
    f_obj_state = QString::number(objection_state);

  // We're doing an Objection (custom objections not yet supported)
  if (objection_state == 2 && Options::getInstance().objectionStopMusic())
    music_stop(true);

  packet_contents.append(f_obj_state);

  if (is_presenting_evidence)
    // the evidence index is shifted by 1 because 0 is no evidence per legacy
    // standards besides, older clients crash if we pass -1
    packet_contents.append(QString::number(current_evidence + 1));
  else
    packet_contents.append("0");

  QString f_flip;

  if (ao_app->flipping_supported) {
    if (ui_flip->isChecked())
      f_flip = "1";
    else
      f_flip = "0";
  }
  else
    f_flip = QString::number(m_cid);

  packet_contents.append(f_flip);

  packet_contents.append(QString::number(realization_state));

  QString f_text_color;

  if (text_color < 0)
    f_text_color = "0";
  else if (text_color >= max_colors)
    f_text_color = "0";
  else
    f_text_color = QString::number(text_color);

  packet_contents.append(f_text_color);

  // If the server we're on supports CCCC stuff, we should use it!
  if (ao_app->cccc_ic_supported) {
    // If there is a showname entered, use that -- else, just send an empty
    // packet-part.
    if (!ui_ic_chat_name->text().isEmpty()) {
      packet_contents.append(ui_ic_chat_name->text());
    }
    else {
      packet_contents.append("");
    }

    // Similarly, we send over whom we're paired with, unless we have chosen
    // ourselves. Or a charid of -1 or lower, through some means.
    if (other_charid > -1 && other_charid != m_cid) {
      QString packet = QString::number(other_charid);
      if (ao_app->effects_supported) // Only servers with effects enabled will
                                   // support pair reordering
        packet += "^" + QString::number(pair_order);
      packet_contents.append(packet);
    }
    else if (ao_app->ulta_supported && ao_app->effects_supported) {
      // No first partner, the order still counts when only the second one is used
      packet_contents.append("-1^" + QString::number(pair_order));
    }
    else {
      packet_contents.append("-1");
    }
    // Send the offset as it's gonna be used regardless
    if(ao_app->y_offset_supported)
        packet_contents.append(QString::number(char_offset) + "&" + QString::number(char_vert_offset));
    else
        packet_contents.append(QString::number(char_offset));

    // Finally, we send over if we want our pres to not interrupt.
    if (ui_immediate->isChecked() && ui_pre->isChecked()) {
      packet_contents.append("1");
    }
    else {
      packet_contents.append("0");
    }
  }

  // If the server we're on supports Looping SFX and Screenshake, use it if the
  // emote uses it.
  if (ao_app->looping_sfx_supported) {
    packet_contents.append(
        ao_app->get_sfx_looping(current_char, current_emote));
    packet_contents.append(QString::number(screenshake_state));

    QString pre_emote = ao_app->get_pre_emote(current_char, current_emote);
    QString emote = ao_app->get_emote(current_char, current_emote);
    QStringList emotes_to_check = {pre_emote, "(b)" + emote, "(a)" + emote};
    QStringList effects_to_check = {"_FrameScreenshake", "_FrameRealization",
                                    "_FrameSFX"};

    foreach (QString f_effect, effects_to_check) {
      QString packet;
      foreach (QString f_emote, emotes_to_check) {
        packet += f_emote;
        if (Options::getInstance().networkedFrameSfxEnabled()) {
          QString sfx_frames =
              ao_app
                  ->read_ini_tags(
                      ao_app->get_character_path(current_char, "char.ini"),
                      f_emote.append(f_effect))
                  .join("|");
          if (sfx_frames != "")
            packet += "|" + sfx_frames;
        }
        packet += "^";
      }
      packet_contents.append(packet);
    }
  }

  if (ao_app->additive_text_supported) {
    packet_contents.append(ui_additive->isChecked() ? "1" : "0");
  }
  if (ao_app->effects_supported) {
    QString p_effect_folder =
        ao_app->read_char_ini(current_char, "effects", "Options");
    QString fx_sound =
        ao_app->get_effect_property(effect, current_char, p_effect_folder, "sound");

    // Don't overlap the two sfx
    if (!ui_pre->isChecked() && (!custom_sfx.isEmpty() || ui_sfx_dropdown->currentIndex() == 1)) {
      fx_sound = "0";
    }

    packet_contents.append(effect + "|" + p_effect_folder + "|" + fx_sound);
    if (!Options::getInstance().clearEffectsDropdownOnPlayEnabled() && !ao_app->get_effect_property(effect, current_char, p_effect_folder, "sticky").startsWith("true")) {
      ui_effects_dropdown->blockSignals(true);
      ui_effects_dropdown->setCurrentIndex(0);
      ui_effects_dropdown->blockSignals(false);
      effect = "";
    }
  }

  // Second pair partner, appended after the 26 standard fields (ULTA servers only, and only if all
  // standard fields were sent)
  if (ao_app->ulta_supported && ao_app->cccc_ic_supported &&
      ao_app->looping_sfx_supported && ao_app->additive_text_supported &&
      ao_app->effects_supported) {
    int partner2 = (other_charid2 > -1 && other_charid2 != m_cid &&
                    other_charid2 != other_charid)
                       ? other_charid2
                       : -1;
    packet_contents.append(QString::number(partner2));
    // The player's own offset, as for the first partner (partners use their own).
    if (ao_app->y_offset_supported)
      packet_contents.append(QString::number(char_offset) + "&" +
                             QString::number(char_vert_offset));
    else
      packet_contents.append(QString::number(char_offset));
    // Protocol v2: the player's custom text colors ("-" = none). Not '#'-prefixed
    // because '#' is the packet separator.
    packet_contents.append(ulta_palette_string());
    // Protocol v3: narrator mode ("-" = not narrating).
    QString f_narration = ulta_narration_string();
    if (!m_pending_fx_token.isEmpty() && f_narration != "-")
      f_narration += ";" + m_pending_fx_token;
    m_pending_fx_token.clear();
    packet_contents.append(f_narration);
    // Protocol v5: character fade for this line ("-" = none), one-shot.
    packet_contents.append(ulta_charfx_next == "o" && ulta_charfx_keep
                               ? QString("O")
                               : ulta_charfx_next);
    if (ulta_charfx_next != "-") {
      ulta_charfx_next = "-";
      update_charfade_button();
    }
  }

  ao_app->send_server_packet(new AOPacket("MS", packet_contents));
}

void Courtroom::reset_ui()
{
  ui_ic_chat_message->clear();
  if (ui_additive->isChecked())
    ui_ic_chat_message->insert(" ");
  objection_state = 0;
  realization_state = 0;
  screenshake_state = 0;
  is_presenting_evidence = false;
  ui_hold_it->set_image("holdit");
  ui_objection->set_image("objection");
  ui_take_that->set_image("takethat");
  ui_custom_objection->set_image("custom");
  ui_realization->set_image("realization");
  ui_screenshake->set_image("screenshake");
  ui_evidence_present->set_image("present");

  // If sticky sounds is disabled and we either have SFX on Idle enabled, or our Preanim checkbox is checked
  if (!Options::getInstance().clearSoundsDropdownOnPlayEnabled() && (Options::getInstance().playSelectedSFXOnIdle() || ui_pre->isChecked())) {
    // Reset the SFX Dropdown to "Default"
    ui_sfx_dropdown->setCurrentIndex(0);
    ui_sfx_remove->hide();
    custom_sfx = "";
  }
  // If sticky preanims is disabled
  if (!Options::getInstance().clearPreOnPlayEnabled())
    // Turn off our Preanim checkbox
    ui_pre->setChecked(false);
}

void Courtroom::chatmessage_enqueue(QStringList p_contents)
{
  // Instead of checking for whether a message has at least chatmessage_size
  // amount of packages, we'll check if it has at least 15.
  // That was the original chatmessage_size.
  if (p_contents.size() < MS_MINIMUM)
    return;

  // A narrated message is logged (and shown) under the narrator's name
  if (ao_app->ulta_supported && p_contents.size() > NARRATION) {
    bool n_on = false;
    QString n_folder, n_name;
    int n_scene = 0;
    parse_narration(p_contents[NARRATION], n_on, n_folder, n_scene, n_name);
    if (n_on)
      p_contents[SHOWNAME] = n_name;
  }

  // Check the validity of the character ID we got
  int f_char_id = p_contents[CHAR_ID].toInt();
  if (f_char_id < -1 || f_char_id >= char_list.size())
    return;

  // We muted this char, gtfo
  if (mute_map.value(f_char_id))
    return;

  // Use null showname if packet does not support 2.6+ extensions
  QString showname = QString();
  if (SHOWNAME < p_contents.size())
    showname = p_contents[SHOWNAME];

  // if the char ID matches our client's char ID (most likely, this is our message coming back to us)
  bool sender = f_char_id == m_cid;

  // Record the log I/O, log files should be accurate.
  LogMode log_mode = IO_ONLY;

  // User-created blankpost
  if (p_contents[MESSAGE].trimmed().isEmpty()) {
    // Turn it into true blankpost
    p_contents[MESSAGE] = "";
  }

  // If we determine we sent this message
  if (sender) {
    // Reset input UI elements, clear input box, etc.
    reset_ui();
  }
  // If we determine we sent this message, or we have desync enabled
  if (sender || Options::getInstance().desynchronisedLogsEnabled()) {
    // Initialize operation "message queue ghost"
    log_chatmessage(p_contents[MESSAGE], p_contents[CHAR_ID].toInt(),
                    p_contents[SHOWNAME], p_contents[CHAR_NAME],
                    p_contents[OBJECTION_MOD], p_contents[EVIDENCE_ID].toInt(),
                    p_contents[TEXT_COLOR].toInt(), QUEUED, sender || Options::getInstance().desynchronisedLogsEnabled(), p_contents.value(PAL_OVERRIDES));
  }

  bool is_objection = false;
  // If the user wants to clear queue on objection
  if (Options::getInstance().objectionSkipQueueEnabled())
  {
    int objection_mod = p_contents[OBJECTION_MOD].split("&")[0].toInt();
    is_objection = objection_mod >= 1 && objection_mod <= 5;
    // If this is an objection, nuke the queue
    if (is_objection) {
      text_queue_timer->stop();
      skip_chatmessage_queue();
    }
  }
  // Log the IO file
  log_chatmessage(p_contents[MESSAGE], f_char_id, showname, p_contents[CHAR_NAME], p_contents[OBJECTION_MOD], p_contents[EVIDENCE_ID].toInt(), p_contents[TEXT_COLOR].toInt(), log_mode, sender, p_contents.value(PAL_OVERRIDES));

  // Send this boi into the queue
  chatmessage_queue.enqueue(p_contents);

  // Our settings disabled queue, or no message is being parsed right now and we're not waiting on one
  bool start_queue = Options::getInstance().textStayTime() <= 0 || (text_state >= 2 && !text_queue_timer->isActive());
  // Objections also immediately play the message
  if (is_objection) {
    m_wtce_busy = false;
    m_wtce_resume = false;
  }
  if (start_queue || is_objection)
    chatmessage_dequeue(); // Process the message instantly
  // Otherwise, since a message is being parsed, chat_tick() should be called which will call dequeue
  // once it's done.
}

void Courtroom::chatmessage_dequeue()
{
  // Nothing to parse in the queue
  if (chatmessage_queue.isEmpty())
    return;

  // Stop the text queue timer
  if (text_queue_timer->isActive())
    text_queue_timer->stop();

  unpack_chatmessage(chatmessage_queue.dequeue());
}

void Courtroom::skip_chatmessage_queue()
{
  while (!chatmessage_queue.isEmpty()) {
    QStringList p_contents = chatmessage_queue.dequeue();
    // if the char ID matches our client's char ID (most likely, this is our message coming back to us)
    bool sender = Options::getInstance().desynchronisedLogsEnabled() || p_contents[CHAR_ID].toInt() == m_cid;
    log_chatmessage(p_contents[MESSAGE], p_contents[CHAR_ID].toInt(), p_contents[SHOWNAME], p_contents[CHAR_NAME], p_contents[OBJECTION_MOD], p_contents[EVIDENCE_ID].toInt(), p_contents[TEXT_COLOR].toInt(), DISPLAY_ONLY, sender, p_contents.value(PAL_OVERRIDES));
  }
}

void Courtroom::unpack_chatmessage(QStringList p_contents)
{
  for (int n_string = 0; n_string < MS_MAXIMUM; ++n_string) {
    // Note that we have added stuff that vanilla clients and servers simply
    // won't send. So now, we have to check if the thing we want even exists
    // amongst the packet's content. We also have to check if the server even
    // supports CCCC's IC features, or if it's just japing us. Also, don't
    // forget! A size 15 message will have indices from 0 to 14.
    if (n_string < p_contents.size() &&
        (n_string < MS_MINIMUM || ao_app->cccc_ic_supported)) {
      m_chatmessage[n_string] = p_contents.at(n_string);
    }
    else {
      m_chatmessage[n_string] = "";
    }
  }

  // Sender's custom text colors for this message (empty = none)
  m_msg_palette = parse_palette(m_chatmessage[PAL_OVERRIDES],
                                m_chatmessage[CHAR_ID].toInt() == m_cid,
                                message_background_color());
  m_msg_statement = ao_app->ulta_supported &&
                    palette_marks_statement(m_chatmessage[PAL_OVERRIDES]);

  // Narrator mode of this message (chat style, voice, name, scene)
  m_narr_on = false;
  QString tool_token;
  if (ao_app->ulta_supported)
    parse_narration(m_chatmessage[NARRATION], m_narr_on, m_narr_folder,
                    m_narr_scene, m_narr_name, &tool_token);
  {
    // Scene effects: this message starts or ends one, or another character speaking ends it (unless held)
    static const QRegularExpression fx_re("^[bw][oi]h?$");
    const int speaker_cid = m_chatmessage[CHAR_ID].toInt();
    const bool fx_token = fx_re.match(tool_token).hasMatch();
    if (m_narr_on && fx_token)
      apply_scene_fx(tool_token, speaker_cid);
    else if (m_narr_on && tool_token.startsWith('t'))
      play_tool_overlay(tool_token.mid(1).toInt());
    else if (m_fade_on && !m_fade_hold && speaker_cid != m_fade_owner)
      clear_scene_fx();
    // A flashback ends when a different character speaks, unless it is held.
    if (m_flash_on && !m_flash_hold && speaker_cid != m_flash_owner)
      clear_flashback_local();
  }
  // The narrator speaks, not the character. Pre-animation, zoom and sound stay the sender's own
  // choice, the character just never "talks"
  if (m_narr_on)
    m_chatmessage[SHOWNAME] = m_narr_name;

  // if the char ID matches our client's char ID (most likely, this is our message coming back to us)
  bool sender = Options::getInstance().desynchronisedLogsEnabled() || m_chatmessage[CHAR_ID].toInt() == m_cid;

  // We have logs displaying as soon as we reach the message in our queue, which is a less confusing
  // but also less accurate experience for the user.
  log_chatmessage(m_chatmessage[MESSAGE], m_chatmessage[CHAR_ID].toInt(), m_chatmessage[SHOWNAME], m_chatmessage[CHAR_NAME], m_chatmessage[OBJECTION_MOD], m_chatmessage[EVIDENCE_ID].toInt(), m_chatmessage[TEXT_COLOR].toInt(), DISPLAY_ONLY, sender, m_chatmessage[PAL_OVERRIDES]);

  // Process the callwords for this message
  handle_callwords();

  // Reset the interface to make room for objection handling
  ui_vp_chat_arrow->hide();
  text_state = 0;
  anim_state = 0;
  evidence_presented = false;
  ui_vp_objection->stop();
  chat_tick_timer->stop();
  ui_vp_evidence_display->reset();
  // This chat msg is not objection so we're not waiting on the objection animation to finish to
  // display the character.
  if (!handle_objection())
    handle_ic_message();
}

void Courtroom::log_chatmessage(QString f_message, int f_char_id, QString f_showname, QString f_char, QString f_objection_mod, int f_evi_id, int f_color, LogMode f_log_mode, bool sender, QString f_palette)
{
  // Colors for this log line come from the sender's palette (if any)
  m_log_palette = parse_palette(f_palette, f_char_id != -1 && f_char_id == m_cid, log_background_color());
  m_log_statement = ao_app->ulta_supported && palette_marks_statement(f_palette);
  auto palette_guard = qScopeGuard([this]() {
    m_log_palette.clear();
    m_log_statement = false;
  });

  // Display name will use the showname
  QString f_displayname = f_showname;
  if (f_char_id != -1) {
    // Grab the char.ini showname
    f_showname = ao_app->get_showname(char_list.at(f_char_id).name);
  }
  // If display name is just whitespace, use the char.ini showname.
  if (f_displayname.trimmed().isEmpty())
    f_displayname = f_showname;

  bool ghost = f_log_mode == QUEUED;
  // Detect if we're trying to log a blankpost
  bool blankpost = (f_log_mode != IO_ONLY && // if we're not in I/O only mode,
                   f_message.isEmpty() &&  // our current message is a blankpost,
                   !ic_chatlog_history.isEmpty() && // the chat log isn't empty,
                   last_ic_message == f_displayname + ":" && // the chat log's last message is a blank post, and
                   last_ic_message.mid(0, last_ic_message.lastIndexOf(":")) == f_displayname); // the blankpost's showname is the same as ours
  bool selfname = f_char_id == m_cid;

  if (log_ic_actions) {
    // Check if a custom objection is in use
    int objection_mod = 0;
    QString custom_objection = "";
    if (f_objection_mod.contains("4&")) {
      objection_mod = 4;
      custom_objection = f_objection_mod.split(
          "4&")[1]; // takes the name of custom objection.
    }
    else {
      objection_mod = f_objection_mod.toInt();
    }

    //QString f_custom_theme = ao_app->get_chat(f_char);
    if (objection_mod <= 4 && objection_mod >= 1) {
      blankpost = false;
      QString shout_message;
      switch (objection_mod) {
      case 1:
        shout_message = ao_app->read_char_ini(f_char, "holdit_message", "Shouts");
        if (shout_message == "")
          shout_message = tr("HOLD IT!");
        break;
      case 2:
        shout_message = ao_app->read_char_ini(f_char, "objection_message", "Shouts");
        if (shout_message == "")
          shout_message = tr("OBJECTION!");
        break;
      case 3:
        shout_message = ao_app->read_char_ini(f_char, "takethat_message", "Shouts");
        if (shout_message == "")
          shout_message = tr("TAKE THAT!");
        break;
      // case 4 is AO2 only
      case 4:
        if (custom_objection != "") {
          shout_message = ao_app->read_char_ini(f_char, custom_objection.split('.')[0] + "_message", "Shouts");
          if (shout_message == "")
            shout_message = custom_objection.split('.')[0];
        }
        else {
          shout_message = ao_app->read_char_ini(f_char, "custom_message", "Shouts");
          if (shout_message == "")
            shout_message = tr("CUSTOM OBJECTION!");
        }
        break;
      }
      switch (f_log_mode) {
        case IO_ONLY:
          log_ic_text(f_char, f_displayname, shout_message, tr("shouts"), 0, selfname);
          break;
        case DISPLAY_AND_IO:
          log_ic_text(f_char, f_displayname, shout_message, tr("shouts"));
          append_ic_text(shout_message, f_displayname, tr("shouts"),
                         0, selfname, QDateTime::currentDateTime(), false);
          break;
        case DISPLAY_ONLY:
        case QUEUED:
          if (!ghost && sender)
            pop_ic_ghost();
          append_ic_text(shout_message, f_displayname, tr("shouts"),
                         0, selfname, QDateTime::currentDateTime(), ghost);
          break;
      }
    }

    // If the evidence ID is in the valid range
    if (f_evi_id > 0 && f_evi_id <= local_evidence_list.size()) {
      blankpost = false;
      // Obtain the evidence name
      QString f_evi_name = local_evidence_list.at(f_evi_id - 1).name;
      switch (f_log_mode) {
        case IO_ONLY:
          log_ic_text(f_showname, f_displayname, f_evi_name, tr("has presented evidence"), 0, selfname);
          break;
        case DISPLAY_AND_IO:
          log_ic_text(f_showname, f_displayname, f_evi_name, tr("has presented evidence"));
          append_ic_text(f_evi_name, f_displayname, tr("has presented evidence"),
                         0, selfname, QDateTime::currentDateTime(), false);
          break;
        case DISPLAY_ONLY:
        case QUEUED:
          if (!ghost && sender)
            pop_ic_ghost();
          append_ic_text(f_evi_name, f_displayname, tr("has presented evidence"),
                         0, selfname, QDateTime::currentDateTime(), ghost);
          break;
      }
    }
  }

  // Do not display anything on a repeated blankpost if it's not a ghost
  if (blankpost && !ghost) {
    // Don't forget to clear the ghost if it's us
    if (sender)
      pop_ic_ghost();
    return;
  }

  switch (f_log_mode) {
    case IO_ONLY:
      log_ic_text(f_showname, f_displayname, f_message, "", f_color, selfname);
      break;
    case DISPLAY_AND_IO:
      log_ic_text(f_showname, f_displayname, f_message, "", f_color, selfname);
      append_ic_text(f_message, f_displayname, "",
                     f_color, selfname, QDateTime::currentDateTime(), false);
      break;
    case DISPLAY_ONLY:
    case QUEUED:
      if (!ghost && sender)
        pop_ic_ghost();
      append_ic_text(f_message, f_displayname, "",
                     f_color, selfname, QDateTime::currentDateTime(), ghost);
      break;
  }
}

bool Courtroom::handle_objection()
{
  // Check if a custom objection is in use
  int objection_mod = 0;
  QString custom_objection = "";
  if (m_chatmessage[OBJECTION_MOD].contains("4&")) {
    objection_mod = 4;
    custom_objection = m_chatmessage[OBJECTION_MOD].split(
        "4&")[1]; // takes the name of custom objection.
  }
  else {
    objection_mod = m_chatmessage[OBJECTION_MOD].toInt();
  }

  // if an objection is used
  if (objection_mod <= 4 && objection_mod >= 1) {
    ui_vp_objection->set_static_duration(shout_static_time);
    ui_vp_objection->set_max_duration(shout_max_time);
    QString filename;
    switch (objection_mod) {
    case 1:
      filename = "holdit_bubble";
      objection_player->play("holdit", m_chatmessage[CHAR_NAME],
                             ao_app->get_chat(style_char()));
      break;
    case 2:
      filename = "objection_bubble";
      objection_player->play("objection", m_chatmessage[CHAR_NAME],
                             ao_app->get_chat(style_char()));
      break;
    case 3:
      filename = "takethat_bubble";
      objection_player->play("takethat", m_chatmessage[CHAR_NAME],
                             ao_app->get_chat(style_char()));
      break;
    // case 4 is AO2 only
    case 4:
      if (custom_objection != "") {
        filename = "custom_objections/" + custom_objection.left(custom_objection.lastIndexOf("."));
        objection_player->play(filename,
            m_chatmessage[CHAR_NAME],
            ao_app->get_chat(style_char()));
      }
      else {
        filename = "custom";
        objection_player->play(
            "custom", m_chatmessage[CHAR_NAME],
            ao_app->get_chat(style_char()));
      }
      break;
      m_chatmessage[EMOTE_MOD] = QChar(static_cast<int>(PREANIM));
    }
    ui_vp_objection->load_image(
        filename, m_chatmessage[CHAR_NAME],
        ao_app->get_chat(style_char()));
    sfx_player->clear(); // Objection played! Cut all sfx.
    ui_vp_player_char->set_play_once(true);
    return true;
  }
  if (m_chatmessage[EMOTE] != "")
    display_character();
  return false;
}

void Courtroom::display_character()
{
  // Stop all previously playing animations, effects etc.
  ui_vp_speedlines->hide();
  ui_vp_player_char->stop();
  ui_vp_effect->stop();
  // Clear all looping sfx to prevent obnoxiousness
  sfx_player->loop_clear();
  // Hide the message and chatbox and handle the emotes
  ui_vp_message->hide();
  ui_vp_chatbox->setVisible(chatbox_always_show);
  // Show it if chatbox always shows
  if (Options::getInstance().characterStickerEnabled() && chatbox_always_show) {
    ui_vp_sticker->load_image(m_chatmessage[CHAR_NAME]);
  }
  // Hide the face sticker
  else {
    ui_vp_sticker->stop();
  }

  // Arrange the netstrings of the frame SFX for the character to know about
  if (!m_chatmessage[FRAME_SFX].isEmpty() &&
      Options::getInstance().networkedFrameSfxEnabled()) {
    // ORDER IS IMPORTANT!!
    QStringList netstrings = {m_chatmessage[FRAME_SCREENSHAKE],
                              m_chatmessage[FRAME_REALIZATION],
                              m_chatmessage[FRAME_SFX]};
    ui_vp_player_char->network_strings = netstrings;
  }
  else
    ui_vp_player_char->network_strings.clear();

  // Determine if we should flip the character or not
  ui_vp_player_char->set_flipped(m_chatmessage[FLIP].toInt() == 1);
  // Move the character on the viewport according to the offsets
  set_self_offset(m_chatmessage[SELF_OFFSET]);
}

void Courtroom::display_pair_character(QString other_charid, QString other_offset)
{
  // If pair information exists
  if (!other_charid.isEmpty()) {
    // Initialize the "ok" bool check to see if the toInt conversion succeeded
    bool ok;
    // Grab the charid of the pair
    int charid = other_charid.split("^")[0].toInt(&ok);
    // If the charid is an int and is valid...
    if (ok && charid > -1) {
      // A character kept faded out stays hidden as a partner too
      if (ao_app->ulta_supported && (m_charfx_gone.contains(charid) ||
                                     m_charfx_away.contains(charid))) {
        ui_vp_sideplayer_char->hide();
        return;
      }
      // Show the pair character
      ui_vp_sideplayer_char->show();
      // Obtain the offsets, splitting it up by & char
      QStringList offsets = other_offset.split("&");
      int offset_x;
      int offset_y;
      // If we only got one number...
      if (offsets.length() <= 1) {
        // That's just the X offset. Make Y offset 0.
        offset_x = other_offset.toInt();
        offset_y = 0;
      }
      else {
        // We got two numbers, set x and y offsets!
        offset_x = offsets[0].toInt();
        offset_y = offsets[1].toInt();
      }
      // Move pair character according to the offsets
      ui_vp_sideplayer_char->move(ui_viewport->width() * offset_x / 100,
                                  ui_viewport->height() * offset_y / 100);
      // Split the charid according to the ^ to determine if we have "ordering" info
      QStringList args = other_charid.split("^");
      if (args.size() >
          1) // This ugly workaround is so we don't make an extra packet just
              // for this purpose. Rewrite pairing when?
      {
        // Change the order of appearance based on the pair order variable
        int order = args.at(1).toInt();
        switch (order) {
        case 0: // Our character is in front
          ui_vp_sideplayer_char->stackUnder(ui_vp_player_char);
          break;
        case 1: // Our character is behind
          ui_vp_player_char->stackUnder(ui_vp_sideplayer_char);
          break;
        default:
          break;
        }
      }
      // Flip the pair character
      if (ao_app->flipping_supported && m_chatmessage[OTHER_FLIP].toInt() == 1)
        ui_vp_sideplayer_char->set_flipped(true);
      else
        ui_vp_sideplayer_char->set_flipped(false);

      // Play the other pair character's idle animation
      QString filename = "(a)" + m_chatmessage[OTHER_EMOTE];
      ui_vp_sideplayer_char->set_play_once(false);
      ui_vp_sideplayer_char->load_image(filename, m_chatmessage[OTHER_NAME],
                                            0, false);
      }
    }
}

void Courtroom::display_pair_character2()
{
  // Only ULTA servers send the second-partner fields.
  if (!ao_app->ulta_supported)
    return;

  // Is the first partner on screen? (same test display_pair_character uses)
  bool ok1 = false;
  int charid1 = m_chatmessage[OTHER_CHARID].split("^")[0].toInt(&ok1);
  bool p1_shown = ok1 && charid1 > -1 && !m_charfx_gone.contains(charid1) &&
                  !m_charfx_away.contains(charid1);

  bool ok2 = false;
  int charid2 = m_chatmessage[PAIR2_CHARID].toInt(&ok2);
  bool p2_shown = ok2 && charid2 > -1 && !m_chatmessage[PAIR2_NAME].isEmpty() &&
                  !m_charfx_gone.contains(charid2) &&
                  !m_charfx_away.contains(charid2);

  if (p2_shown) {
    ui_vp_sideplayer2_char->show();

    // Offsets arrive as "x" or "x&y", same as the first partner.
    QStringList offsets = m_chatmessage[PAIR2_OFFSET].split("&");
    int offset_x = offsets.at(0).toInt();
    int offset_y = offsets.length() > 1 ? offsets.at(1).toInt() : 0;
    ui_vp_sideplayer2_char->move(ui_viewport->width() * offset_x / 100,
                                 ui_viewport->height() * offset_y / 100);

    ui_vp_sideplayer2_char->set_flipped(ao_app->flipping_supported &&
                                        m_chatmessage[PAIR2_FLIP].toInt() == 1);

    // Play the partner's idle animation, like the first partner.
    QString filename = "(a)" + m_chatmessage[PAIR2_EMOTE];
    ui_vp_sideplayer2_char->set_play_once(false);
    ui_vp_sideplayer2_char->load_image(filename, m_chatmessage[PAIR2_NAME], 0,
                                       false);
  }

  apply_pair_stacking(p1_shown, p2_shown);
}

void Courtroom::apply_pair_stacking(bool p1_shown, bool p2_shown)
{
  // Order from the message: 0 = our character in front, 1 = behind,
  // 2 = in the middle. Layers are listed front to back below.
  int order = m_chatmessage[PAIR_ORDER_REAL].toInt();
  CharLayer *me = ui_vp_player_char;
  CharLayer *s1 = ui_vp_sideplayer_char;
  CharLayer *s2 = ui_vp_sideplayer2_char;

  if (p1_shown && p2_shown) {
    switch (order) {
    case 1: // partner 1, partner 2, me
      s2->stackUnder(s1);
      me->stackUnder(s2);
      break;
    case 2: // partner 1, me, partner 2
      me->stackUnder(s1);
      s2->stackUnder(me);
      break;
    default: // me, partner 1, partner 2
      s1->stackUnder(me);
      s2->stackUnder(s1);
      break;
    }
  }
  else if (p2_shown) {
    // Only the second partner: behaves like a stock pair.
    if (order == 1)
      me->stackUnder(s2);
    else
      s2->stackUnder(me);
  }
  // Only the first partner: display_pair_character() already stacked it.
}

void Courtroom::handle_emote_mod(int emote_mod, bool p_immediate)
{
  // Deal with invalid emote modifiers
  if (emote_mod != IDLE && emote_mod != PREANIM && emote_mod != ZOOM && emote_mod != PREANIM_ZOOM) {
    // If emote mod is 4...
    if (emote_mod == 4) {
      emote_mod = PREANIM_ZOOM; // Addresses issue with an old bug that sent the wrong
                     // emote modifier for zoompre
    }
    else if (emote_mod == 2) {
      // Addresses the deprecated "objection preanim"
      emote_mod = PREANIM;
    }
    else {
      emote_mod = IDLE; // Reset emote mod to 0
    }
  }

  // Handle the emote mod
  switch (emote_mod) {
  case PREANIM:
  case PREANIM_ZOOM:
    // play preanim that makes the chatbox wait for it to finish.
    play_preanim(false);
    break;
  case IDLE:
  case ZOOM:
    // If immediate is not ticked on...
    if (!p_immediate)
    {
      // Skip preanim.
      handle_ic_speaking();
    }
    else
    {
      // IDLE, ZOOM both play preanim alongside the chatbox, not waiting for the animation to finish.
      // This behavior is a bit jank (why is emote mod affected by immediate?) but eh it functions
      play_preanim(true);
    }
    break;
  default:
    // This should never happen, but if it does anyway, yell in the console about it.
    qWarning() << "invalid emote mod: " << QString::number(emote_mod);
  }
}

void Courtroom::objection_done() { handle_ic_message(); }

void Courtroom::handle_ic_message()
{
  // Update the chatbox information
  initialize_chatbox();

  int emote_mod = m_chatmessage[EMOTE_MOD].toInt();
  bool immediate = m_chatmessage[IMMEDIATE].toInt() == 1;
  // Every message starts with the character layers allowed to show, an "alone" narration then keeps
  // them hidden for the whole message
  reset_char_fx();
  ui_vp_player_char->force_hidden = false;
  ui_vp_sideplayer_char->force_hidden = false;
  ui_vp_sideplayer2_char->force_hidden = false;
  if (m_narr_on && m_narr_scene == 1) {
    // "Alone": only the background and the narration (the next message shows the layers again)
    ui_vp_player_char->stop();
    ui_vp_player_char->hide();
    ui_vp_player_char->force_hidden = true;
    ui_vp_sideplayer_char->stop();
    ui_vp_sideplayer_char->hide();
    ui_vp_sideplayer_char->force_hidden = true;
    ui_vp_sideplayer2_char->stop();
    ui_vp_sideplayer2_char->hide();
    ui_vp_sideplayer2_char->force_hidden = true;
    play_sfx();
    start_chat_ticking();
  }
  else if (m_chatmessage[EMOTE] != "") {
    ui_vp_player_char->show(); // may have been hidden by an "alone" narration
    // The speaker's own character fades in / out with this line
    if (ao_app->ulta_supported) {
      const int fx_cid = m_chatmessage[CHAR_ID].toInt();
      const QString fx = m_chatmessage[CHAR_FX];
      // Their own line brings them back from a plain fade out
      if (fx != "o" && fx != "O")
        m_charfx_away.remove(fx_cid);
      if (fx == "i") {
        m_charfx_gone.remove(fx_cid);
        start_char_fx_in();
      }
      else if (fx == "o" || fx == "O") {
        // "O" = fade out and stay gone through later lines (unseen voice) until a fade in, "o" =
        // hidden until they speak again
        if (fx == "O")
          m_charfx_gone.insert(fx_cid);
        else {
          m_charfx_gone.remove(fx_cid);
          m_charfx_away.insert(fx_cid);
        }
        m_charfx_out_pending = true;
      }
      else if (m_charfx_gone.contains(fx_cid)) {
        // Still faded out from an earlier line.
        ui_vp_player_char->setLayerOpacity(0.0);
      }
      if (fx_cid == m_cid)
        update_charfade_button();
    }
    // Display our own character
    display_character();

    // Reset the pair character
    ui_vp_sideplayer_char->stop();
    ui_vp_sideplayer_char->move(0, 0);
    ui_vp_sideplayer2_char->stop();
    ui_vp_sideplayer2_char->hide();
    ui_vp_sideplayer2_char->move(0, 0);

    // If the emote_mod is not zooming
    if (emote_mod != ZOOM && emote_mod != PREANIM_ZOOM) {
      // Display the pair character
      display_pair_character(m_chatmessage[OTHER_CHARID], m_chatmessage[OTHER_OFFSET]);
      // Second partner + three-way stacking (no-op on stock servers)
      display_pair_character2();
    }

    // Parse the emote_mod part of the chat message
    handle_emote_mod(emote_mod, immediate);
  }
  else
  {
    play_sfx();
    start_chat_ticking();
  }

  // if we have instant objections disabled, and queue is not empty, check if next message after this
  // is an objection.
  if (!Options::getInstance().objectionSkipQueueEnabled() && chatmessage_queue.size() > 0)
  {
    QStringList p_contents = chatmessage_queue.head();
    int objection_mod = p_contents[OBJECTION_MOD].split("&")[0].toInt();
    bool is_objection = objection_mod >= 1 && objection_mod <= 5;
    // If this is an objection, we'll need to interrupt our current message.
    if (is_objection)
      text_queue_timer->start(objection_threshold);
  }
}

void Courtroom::do_screenshake()
{
  if (!Options::getInstance().shakeEnabled())
    return;

  // This way, the animation is reset in such a way that last played screenshake
  // would return to its "final frame" properly. This properly resets all UI
  // elements without having to bother keeping track of "origin" positions.
  // Works great with the chat text being detached from the chat box!
  screenshake_animation_group->setCurrentTime(
      screenshake_animation_group->duration());
  screenshake_animation_group->clear();

  const QList<QWidget *> &affected_list = {ui_vp_background, ui_vp_player_char,
                                    ui_vp_sideplayer_char,
                                    ui_vp_sideplayer2_char, ui_vp_chatbox};

  // I would prefer if this was its own "shake" function to be honest.
  for (QWidget *ui_element : affected_list) {
    QPropertyAnimation *screenshake_animation =
        new QPropertyAnimation(ui_element, "pos", this);
    QPoint pos_default = QPoint(ui_element->x(), ui_element->y());

    int duration = 300; // How long does the screenshake last
    int frequency = 20; // How often in ms is there a "jolt" frame
    // Maximum deviation from the origin position. This is 7 pixels for a 256x192 viewport,
    // so we scale that value in accordance with the current viewport height so the shake
    // is roughly the same intensity regardless of viewport size. Done as a float operation for maximum
    // accuracy.
    int max_deviation = 7 * (float(ui_viewport->height()) / 192);
    int maxframes = 15; // duration / frequency;
    screenshake_animation->setDuration(duration);
    for (int frame = 0; frame < maxframes; frame++) {
      double fraction = double(frame * frequency) / duration;
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
      int rand_x = max_deviation * (2 * (qrand() / (float)RAND_MAX) - 1) + 1;
      int rand_y = max_deviation * (2 * (qrand() / (float)RAND_MAX) - 1) + 1;
#else
      int rand_x = QRandomGenerator::system()->bounded(-max_deviation, max_deviation);
      int rand_y = QRandomGenerator::system()->bounded(-max_deviation, max_deviation);
#endif
      screenshake_animation->setKeyValueAt(fraction, QPoint(pos_default.x() + rand_x, pos_default.y() + rand_y));
    }
    screenshake_animation->setEndValue(pos_default);
    screenshake_animation->setEasingCurve(QEasingCurve::Linear);
    screenshake_animation_group->addAnimation(screenshake_animation);
  }

  screenshake_animation_group->start();
}

void Courtroom::do_flash()
{
  if (!Options::getInstance().effectsEnabled())
    return;

  QString f_char = m_chatmessage[CHAR_NAME];
  QString f_custom_theme = ao_app->get_chat(f_char);
  do_effect("realization", "", f_char, f_custom_theme);
}

void Courtroom::do_effect(QString fx_path, QString fx_sound, QString p_char,
                          QString p_folder)
{
  if (fx_path == "") {
    return;
  }
  QString effect = ao_app->get_effect(fx_path, p_char, p_folder);
  if (effect == "") {
    return;
  }

  if (fx_sound != "") {
    sfx_player->play(fx_sound);
  }

  // Only check if effects are disabled after playing the sound if it exists
  if (!Options::getInstance().effectsEnabled()) {
    return;
  }
  ui_vp_effect->transform_mode = ao_app->get_scaling(
      ao_app->get_effect_property(fx_path, p_char, p_folder, "scaling"));
  ui_vp_effect->stretch =
      ao_app->get_effect_property(fx_path, p_char, p_folder, "stretch")
          .startsWith("true");
  ui_vp_effect->set_flipped(ao_app->get_effect_property(fx_path, p_char, p_folder, "respect_flip").startsWith("true") && m_chatmessage[FLIP].toInt() == 1);
  ui_vp_effect->set_play_once(
      false); // The effects themselves dictate whether or not they're looping.
              // Static effects will linger.

  bool looping =
      ao_app->get_effect_property(fx_path, p_char, p_folder, "loop")
          .startsWith("true");

  int max_duration =
      ao_app->get_effect_property(fx_path, p_char, p_folder, "max_duration")
          .toInt();

  bool cull =
      ao_app->get_effect_property(fx_path, p_char, p_folder, "cull")
          .startsWith("true");

  // Possible values: "chat", "character", "behind"
  QString layer = ao_app->get_effect_property(fx_path, p_char, p_folder, "layer").toLower();
  if (layer == "behind"){
    ui_vp_effect->setParent(ui_viewport);
    ui_vp_effect->stackUnder(ui_vp_player_char);
  }
  else if (layer == "character") {
    ui_vp_effect->setParent(ui_viewport);
    ui_vp_effect->stackUnder(ui_vp_desk);
  }
  else if (layer == "over") {
    ui_vp_effect->setParent(ui_viewport);
    ui_vp_effect->raise();
  }
  else { // if (layer == "chat") {
    ui_vp_effect->setParent(this);
    ui_vp_effect->stackUnder(ui_vp_objection);
  }

  int effect_x = 0;
  int effect_y = 0;
  // The effect is not parented to viewport, meaning we're overlaying ui elements
  if (ui_vp_effect->parentWidget() != ui_viewport) {
    //We need to add the viewport as an offset as effects are not bound to it.
    effect_x = ui_viewport->x();
    effect_y = ui_viewport->y();
  }
  // This effect respects the character offset settings
  if (ao_app->get_effect_property(fx_path, p_char, p_folder, "respect_offset") == "true") {
    QStringList self_offsets = m_chatmessage[SELF_OFFSET].split("&");
    int self_offset = self_offsets[0].toInt();
    int self_offset_v;
    if (self_offsets.length() <= 1) {
      self_offset_v = 0;
    }
    else {
      self_offset_v = self_offsets[1].toInt();
    }

    // Move the effects layer to match the position of our character
    const int percent = 100;
    effect_x += ui_viewport->width() * self_offset / percent;
    effect_y += ui_viewport->height() * self_offset_v / percent;
  }
  ui_vp_effect->move(effect_x, effect_y);

  ui_vp_effect->set_static_duration(max_duration);
  ui_vp_effect->set_max_duration(max_duration);
  ui_vp_effect->load_image(effect, looping);
  ui_vp_effect->set_cull_image(cull);
}

void Courtroom::play_char_sfx(QString sfx_name)
{
  sfx_player->play(sfx_name);
}

void Courtroom::initialize_chatbox()
{

  int f_charid = m_chatmessage[CHAR_ID].toInt();
  if (m_narr_on) {
    // Narration always shows under the narrator's name
    ui_vp_showname->setText(m_narr_name);
  }
  else if (f_charid >= 0 && f_charid < char_list.size() &&
      (m_chatmessage[SHOWNAME].isEmpty() || !ui_showname_enable->isChecked())) {
    QString real_name = char_list.at(f_charid).name;
    QString f_showname = ao_app->get_showname(real_name);

    ui_vp_showname->setText(f_showname);
  }
  else {
    ui_vp_showname->setText(m_chatmessage[SHOWNAME]);
  }
  QString customchar;
  if (Options::getInstance().customChatboxEnabled())
    customchar = style_char();
  QString p_misc = ao_app->get_chat(customchar);

  set_size_and_pos(ui_vp_chatbox, "ao2_chatbox", p_misc);
  set_size_and_pos(ui_vp_showname, "showname", p_misc);
  set_size_and_pos(ui_vp_message, "message", p_misc);

  QString result = ao_app->get_design_element("chatbox_always_show", "courtroom_design.ini", p_misc);
  chatbox_always_show = result == "1" || result.startsWith("true");

  // We detached the text as parent from the chatbox so it doesn't get affected
  // by the screenshake.
  ui_vp_message->move(ui_vp_message->x() + ui_vp_chatbox->x(),
                      ui_vp_message->y() + ui_vp_chatbox->y());
  ui_vp_message->setTextInteractionFlags(Qt::NoTextInteraction);

  if (ui_vp_showname->text().trimmed().isEmpty()) // Whitespace showname
  {
    ui_vp_chatbox->set_image("chatblank", p_misc);
  }
  else // Aw yeah dude do some showname magic
  {
    if (!ui_vp_chatbox->set_image("chat", p_misc))
      ui_vp_chatbox->set_image("chatbox", p_misc);


    // Remember to set the showname font before the font metrics check.
    set_font(ui_vp_showname, "", "showname", customchar);

    pos_size_type default_width = ao_app->get_element_dimensions(
        "showname", "courtroom_design.ini", p_misc);
    int extra_width =
        ao_app
            ->get_design_element("showname_extra_width", "courtroom_design.ini",
                                 p_misc)
            .toInt();
    QString align = ao_app
                        ->get_design_element("showname_align",
                                             "courtroom_design.ini", p_misc)
                        .toLower();
    if (align == "right")
      ui_vp_showname->setAlignment(Qt::AlignRight);
    else if (align == "center")
      ui_vp_showname->setAlignment(Qt::AlignHCenter);
    else if (align == "justify")
      ui_vp_showname->setAlignment(Qt::AlignHCenter);
    else
      ui_vp_showname->setAlignment(Qt::AlignLeft);

    QFontMetrics fm(ui_vp_showname->font());
// Gotta support the slow paced ubuntu 18 STUCK IN 5.9.5!!
#if QT_VERSION > QT_VERSION_CHECK(5, 11, 0)
    int fm_width = fm.horizontalAdvance(ui_vp_showname->text());
#else
    int fm_width = fm.boundingRect((ui_vp_showname->text())).width();
#endif
    if (extra_width > 0) {
      QString current_path = ui_vp_chatbox->path.left(ui_vp_chatbox->path.lastIndexOf('.'));
      if (fm_width > default_width.width &&
          ui_vp_chatbox->set_image(current_path +
              "med")) // This text be big. Let's do some shenanigans.
      {
        ui_vp_showname->resize(default_width.width + extra_width,
                               ui_vp_showname->height());
        if (fm_width > ui_vp_showname->width() &&
            ui_vp_chatbox->set_image(current_path + "big")) // Biggest possible size for us.
        {
          ui_vp_showname->resize(
              static_cast<int>(default_width.width + (extra_width * 2)),
              ui_vp_showname->height());
        }
      }
      else
        ui_vp_showname->resize(default_width.width, ui_vp_showname->height());
    }
    else {
      ui_vp_showname->resize(default_width.width, ui_vp_showname->height());
    }
  }

  // This should probably be called only if any change from the last chat
  // arrow was actually detected.
  pos_size_type design_ini_result = ao_app->get_element_dimensions(
      "chat_arrow", "courtroom_design.ini", p_misc);
  if (design_ini_result.width < 0 || design_ini_result.height < 0) {
    qWarning() << "could not find \"chat_arrow\" in courtroom_design.ini";
    ui_vp_chat_arrow->hide();
  }
  else {
    ui_vp_chat_arrow->move(design_ini_result.x + ui_vp_chatbox->x(), design_ini_result.y + ui_vp_chatbox->y());
    ui_vp_chat_arrow->combo_resize(design_ini_result.width,
                                    design_ini_result.height);
  }

  QString font_name;
  QString chatfont = ao_app->get_chat_font(m_chatmessage[CHAR_NAME]);
  if (chatfont != "")
    font_name = chatfont;

  int f_pointsize = 0;
  int chatsize = ao_app->get_chat_size(m_chatmessage[CHAR_NAME]);
  if (chatsize > 0)
    f_pointsize = chatsize;
  set_font(ui_vp_message, "", "message", customchar, font_name, f_pointsize);
}

void Courtroom::handle_callwords()
{
  // Quickly check through the message for the word_call (callwords) sfx
  QString f_message = m_chatmessage[MESSAGE];
  //No more file IO on every message.
  QStringList call_words = Options::getInstance().callwords();
  // Loop through each word in the call words list
  for (const QString &word : qAsConst(call_words)) {
    // If our message contains that specific call word
    if (f_message.contains(word, Qt::CaseInsensitive)) {
      // Play the call word sfx on the modcall_player sound container
      modcall_player->play(ao_app->get_court_sfx("word_call"));
      // Make the window flash
      ao_app->alert(this);
      // Break the loop so we don't spam sound effects
      break;
    }
  }
}

void Courtroom::display_evidence_image()
{
  QString side = m_chatmessage[SIDE];
  int f_evi_id = m_chatmessage[EVIDENCE_ID].toInt();
  if (f_evi_id > 0 && f_evi_id <= local_evidence_list.size()) {
    // shifted by 1 because 0 is no evidence per legacy standards
    QString f_image = local_evidence_list.at(f_evi_id - 1).image;
    //QString f_evi_name = local_evidence_list.at(f_evi_id - 1).name;
    // def jud and hlp should display the evidence icon on the RIGHT side
    bool is_left_side = !(side == "def" || side == "hlp" ||
                          side == "jud" || side == "jur");
    ui_vp_evidence_display->show_evidence(f_evi_id, f_image, is_left_side,
                                          sfx_player->get_volume());
  }
}

void Courtroom::handle_ic_speaking()
{
  QString side = m_chatmessage[SIDE];
  int emote_mod = m_chatmessage[EMOTE_MOD].toInt();
  // emote_mod 5 is zoom and emote_mod 6 is zoom w/ preanim.
  if (emote_mod == ZOOM || emote_mod == PREANIM_ZOOM) {
    // Hide the desks
    ui_vp_desk->hide();

    // Obtain character information for our character
    QString filename;
    // I still hate this hardcoding. If we're on pos pro, hlp and wit, use prosecution_speedlines.
    // Otherwise, defense_speedlines.
    if (side == "pro" || side == "hlp" || side == "wit")
      filename = "prosecution_speedlines";
    else
      filename = "defense_speedlines";
    ui_vp_speedlines->load_image(filename, m_chatmessage[CHAR_NAME], ao_app->get_chat(style_char()));
  }

  // Check if this is a talking color (white text, etc.)
  color_is_talking =
      color_markdown_talking_list.at(m_chatmessage[TEXT_COLOR].toInt()) &&
      !m_narr_on; // ULTA: the character never "talks" during narration
  QString filename;
  // If color is talking, and our state isn't already talking
  if (color_is_talking && text_state == 1 &&
      anim_state < 2)
  {
    // Stop the previous animation and play the talking animation
    ui_vp_player_char->stop();
    ui_vp_player_char->set_play_once(false);
    filename = "(b)" + m_chatmessage[EMOTE];
    ui_vp_player_char->load_image(filename, m_chatmessage[CHAR_NAME], 0, false);
    // Set the anim state accordingly
    anim_state = 2;
  }
  else if (anim_state < 3 &&
           anim_state != 3) // Set it to idle as we're not on that already
  {
    // Stop the previous animation and play the idle animation
    ui_vp_player_char->stop();
    ui_vp_player_char->set_play_once(false);
    filename = "(a)" + m_chatmessage[EMOTE];
    ui_vp_player_char->load_image(filename, m_chatmessage[CHAR_NAME], 0, false);
    // Set the anim state accordingly
    anim_state = 3;
  }

  // Begin parsing through the chatbox message
  start_chat_ticking();
}

QString Courtroom::filter_ic_text(QString p_text, bool html, int target_pos,
                                  int default_color)
{
  QString p_text_escaped;

  int check_pos = 0;
  int check_pos_escaped = 0;
  bool parse_escape_seq = false;
  std::stack<int> ic_color_stack;

  // Text alignment shenanigans. Could make a dropdown for this later, too!
  QString align;
  if (p_text.trimmed().startsWith("~~")) {
    p_text.remove(p_text.indexOf("~~"), 2);
    if (target_pos != -1) {
      target_pos = qMax(0, target_pos - 2);
    }
    align = "center";
  }
  else if (p_text.trimmed().startsWith("~>")) {
    p_text.remove(p_text.indexOf("~>"), 2);
    if (target_pos != -1) {
      target_pos = qMax(0, target_pos - 2);
    }
    align = "right";
  }
  else if (p_text.trimmed().startsWith("<>")) {
    p_text.remove(p_text.indexOf("<>"), 2);
    if (target_pos != -1) {
      target_pos = qMax(0, target_pos - 2);
    }
    align = "justify";
  }

  // If html is enabled, prepare this text to be all ready for it.
  if (html) {
    ic_color_stack.push(default_color);
    QString appendage = QString("<font color=\"") +
                        (m_filter_base_stock ? "$s" : "$c") +
                        QString::number(default_color) + "\">";

    if (!align.isEmpty())
      appendage.prepend("<div align=" + align + ">");

    p_text_escaped.insert(check_pos_escaped, appendage);
    check_pos_escaped += appendage.size();
  }

  // Current issue: does not properly escape html stuff.
  // Solution: probably parse p_text and export into a different string
  // separately, perform some mumbo jumbo to properly adjust string indexes.
  while (check_pos < p_text.size()) {
    QString f_rest = p_text.right(p_text.size() - check_pos);
    QTextBoundaryFinder tbf(QTextBoundaryFinder::Grapheme, f_rest);
    QString f_character;
    int f_char_length;
    int f_char_bytes;

    tbf.toNextBoundary();

    if (tbf.position() == -1)
      f_character = f_rest;
    else
      f_character = f_rest.left(tbf.position());

    //    if (f_character == "&") //oh shit it's probably an escaped html
    //    {
    //      //Skip escaped chars like you would graphemes
    //      QRegularExpression re("&([a-z0-9]+|#[0-9]{1,6}|#x[0-9a-f]{1,6});",
    //      QRegularExpression::CaseInsensitiveOption); QRegularExpressionMatch
    //      match = re.match(f_rest); if (match.hasMatch()) //OH SHIT IT IS,
    //      PANIC, PANIC
    //      {
    //        f_character = match.captured(0); //Phew, we solved the big problem
    //        here.
    //      }
    //    }

    f_char_bytes = f_char_length = f_character.length();

    if (html) {
      f_character = f_character.toHtmlEscaped();
      f_char_length = f_character.length();
    }

    bool color_update = false;
    bool is_end = false;
    bool skip = false;

    if (!parse_escape_seq) {
      if (f_character == "\\") {
        parse_escape_seq = true;
        skip = true;
      }
      // Nothing related to colors here
      else if (f_character == "{" ||
               f_character ==
                   "}") //|| f_character == "@" || f_character == "$")
      {
        skip = true;
      }
      // Parse markdown colors
      else {
        for (int c = 0; c < max_colors; ++c) {
          // Clear the stored optimization information
          QString markdown_start = color_markdown_start_list.at(c);
          QString markdown_end = color_markdown_end_list.at(c);
          if (html) {
            markdown_start = markdown_start.toHtmlEscaped();
            markdown_end = markdown_end.toHtmlEscaped();
          }
          bool markdown_remove = color_markdown_remove_list.at(c);
          if (markdown_start.isEmpty()) // Not defined
            continue;

          if (markdown_end.isEmpty() ||
              markdown_end == markdown_start) //"toggle switch" type
          {
            if (f_character == markdown_start) {
              if (html) {
                // Close the span if inside one of this color, the base color (first entry) is never closed
                if (ic_color_stack.size() > 1 && ic_color_stack.top() == c) {
                  ic_color_stack.pop(); // Cease our coloring
                  is_end = true;
                }
                else {
                  ic_color_stack.push(c); // Begin our coloring
                }
                color_update = true;
              }
              skip = markdown_remove;
              break; // Prevent it from looping forward for whatever reason
            }
          }
          else if (f_character == markdown_start ||
                   (f_character == markdown_end && ic_color_stack.size() > 1 &&
                    ic_color_stack.top() == c)) {
            if (html) {
              if (f_character == markdown_end) {
                ic_color_stack.pop(); // Cease our coloring
                is_end = true;
              }
              else if (f_character == markdown_start) {
                ic_color_stack.push(c); // Begin our coloring
              }
              color_update = true;
            }
            skip = markdown_remove;
            break; // Prevent it from looping forward for whatever reason
          }
        }
        // Parse the newest color stack
        if (color_update && (target_pos <= -1 || check_pos < target_pos)) {
          if (!parse_escape_seq) {
            QString appendage = "</font>";

            if (!ic_color_stack.empty())
              appendage +=
                  QString("<font color=\"") +
                  ((m_filter_base_stock && ic_color_stack.size() == 1) ? "$s" : "$c") +
                  QString::number(ic_color_stack.top()) + "\">";

            if (is_end && !skip) {
              p_text_escaped.insert(check_pos_escaped,
                                    f_character); // Add that char right now
              check_pos_escaped +=
                  f_char_length; // So the closing char is captured too
              skip = true;
            }
            p_text_escaped.insert(check_pos_escaped, appendage);
            check_pos_escaped += appendage.size();
          }
        }
      }
    }
    else {
      if (f_character == "n") // \n, that's a line break son
      {
        QString appendage = "<br/>";
        if (!html) {
          // actual newline commented out
          //          appendage = "\n";
          //          size = 1; //yeah guess what \n is a "single character"
          //          apparently
          appendage = "\\n "; // visual representation of a newline
        }
        p_text_escaped.insert(check_pos_escaped, appendage);
        check_pos_escaped += appendage.size();
        skip = true;
      }
      if (f_character == "s" || f_character == "f" || f_character == "p") // screenshake/flash/pause
        skip = true;

      parse_escape_seq = false;
    }

    // Make all chars we're not supposed to see invisible
    if (target_pos > -1 && check_pos == target_pos) {
      QString appendage = "";
      if (!ic_color_stack.empty()) {
        if (!is_end) // Was our last coloring char ending the color stack or nah
        {
          // God forgive me for my transgressions but I have refactored this
          // whole thing about 25 times and having to refactor it again to more
          // elegantly support this will finally make me go insane.
          color_is_talking =
              color_markdown_talking_list.at(ic_color_stack.top()) &&
              !m_narr_on; // ULTA: the character never "talks" during narration
        }

        // Clean it up, we're done here
        while (!ic_color_stack.empty())
          ic_color_stack.pop();

        appendage += "</font>";
      }
      ic_color_stack.push(
          -1); // Dummy colorstack push for maximum </font> appendage
      appendage += "<font color=\"#00000000\">";
      p_text_escaped.insert(check_pos_escaped, appendage);
      check_pos_escaped += appendage.size();
    }
    if (!skip) {
      p_text_escaped.insert(check_pos_escaped, f_character);
      check_pos_escaped += f_char_length;
    }
    check_pos += f_char_bytes;
  }

  if (!ic_color_stack.empty() && html) {
    p_text_escaped.append("</font>");
  }

  if (html) {
    // Example: https://regex101.com/r/oL4nM9/37 - this replaces
    // excessive/trailing/etc. whitespace with non-breaking space. I WOULD use
    // white-space: pre; stylesheet tag, but for whataver reason it doesn't work
    // no matter where I try it. If somoene else can get that piece of HTML
    // memery to work, please do.
    p_text_escaped.replace(QRegularExpression("^\\s|(?<=\\s)\\s"), "&nbsp;");
    if (!align.isEmpty())
      p_text_escaped.append("</div>");
  }

  return p_text_escaped;
}

void Courtroom::log_ic_text(QString p_name, QString p_showname,
                            QString p_message, QString p_action, int p_color, bool p_selfname)
{
  chatlogpiece log_entry(p_name, p_showname, p_message, p_action, p_color, p_selfname);
  ic_chatlog_history.append(log_entry);
  if (Options::getInstance().logToTextFileEnabled() && !ao_app->log_filename.isEmpty())
    ao_app->append_to_file(log_entry.get_full(), ao_app->log_filename, true);

  while (ic_chatlog_history.size() > log_maximum_blocks &&
         log_maximum_blocks > 0) {
    ic_chatlog_history.removeFirst();
  }
}

void Courtroom::append_ic_text(QString p_text, QString p_name, QString p_action,
                               int color, bool selfname, QDateTime timestamp, bool ghost)
{
  QColor chatlog_color = ao_app->get_color("ic_chatlog_color", "courtroom_fonts.ini");
  QTextCharFormat bold;
  QTextCharFormat normal;
  QTextCharFormat italics;
  QTextCharFormat own_name;
  QTextCharFormat other_name;
  QTextCharFormat timestamp_format;
  QTextCharFormat selftimestamp_format;
  QTextBlockFormat format;
  bold.setFontWeight(QFont::Bold);
  normal.setFontWeight(QFont::Normal);
  italics.setFontItalic(true);
  own_name.setFontWeight(QFont::Bold);
  own_name.setForeground(ao_app->get_color("ic_chatlog_selfname_color", "courtroom_fonts.ini"));
  other_name.setFontWeight(QFont::Bold);
  other_name.setForeground(ao_app->get_color("ic_chatlog_showname_color", "courtroom_fonts.ini"));
  timestamp_format.setForeground(ao_app->get_color("ic_chatlog_timestamp_color", "courtroom_fonts.ini"));
  selftimestamp_format.setForeground(ao_app->get_color("ic_chatlog_selftimestamp_color", "courtroom_fonts.ini"));
  format.setTopMargin(log_margin);
  const QTextCursor old_cursor = ui_ic_chatlog->textCursor();
  const int old_scrollbar_value = ui_ic_chatlog->verticalScrollBar()->value();
  const bool need_newline = !ui_ic_chatlog->document()->isEmpty();
  const int scrollbar_target_value =
      log_goes_downwards ? ui_ic_chatlog->verticalScrollBar()->maximum()
                         : ui_ic_chatlog->verticalScrollBar()->minimum();

  if (ghost) {
    ghost_blocks++;
    chatlog_color.setAlpha(128);
    bold.setForeground(chatlog_color);
    normal.setForeground(chatlog_color);
    italics.setForeground(chatlog_color);
  }
  else {
    last_ic_message = p_name + ":" + p_text;
  }

  ui_ic_chatlog->moveCursor(log_goes_downwards ? QTextCursor::End
                                               : QTextCursor::Start);

  if (!ghost && ghost_blocks > 0) {
    for (int i = 0; i < ghost_blocks; ++i) {
      ui_ic_chatlog->moveCursor(log_goes_downwards ? QTextCursor::PreviousBlock
                                                   : QTextCursor::NextBlock);
    }
    ui_ic_chatlog->moveCursor(log_goes_downwards ? QTextCursor::EndOfBlock
                                                 : QTextCursor::StartOfBlock);
  }

  // Only prepend with newline if log goes downwards
  if (log_goes_downwards && need_newline) {
    ui_ic_chatlog->textCursor().insertBlock(format);
  }


  // Timestamp if we're doing that meme
  if (log_timestamp) {
    // Format the timestamp
    QTextCharFormat format = selfname ? selftimestamp_format : timestamp_format;
    if (timestamp.isValid()) {
      ui_ic_chatlog->textCursor().insertText(
        "[" + timestamp.toString(log_timestamp_format) + "] ", format);
    } else {
      qCritical() << "could not insert invalid timestamp" << timestamp;
    }
  }

  // Format the name of the actor
  QTextCharFormat name_format = selfname ? own_name : other_name;
  ui_ic_chatlog->textCursor().insertText(p_name, name_format);
  // Special case for stopping the music
  if (p_action == tr("has stopped the music")) {
    ui_ic_chatlog->textCursor().insertText(" " + p_action + ".", normal);
  }
  // Make shout text bold
  else if (p_action == tr("shouts") && log_ic_actions) {
    ui_ic_chatlog->textCursor().insertText(" " + p_action + " ", normal);
    if (log_colors && !ghost) {
      ui_ic_chatlog->textCursor().insertHtml(
          "<b>" +
          filter_ic_text(p_text, true, -1, 0)
              .replace(
                  "$c0",
                  chatlog_color.name(QColor::HexArgb)) +
          "</b>");
    }
    else
      ui_ic_chatlog->textCursor().insertText(" " + p_text, italics);
  }
  // If action not blank:
  else if (p_action != "" && log_ic_actions) {
    // Format the action in normal
    ui_ic_chatlog->textCursor().insertText(" " + p_action, normal);
    if (log_newline)
      // For some reason, we're forced to use <br> instead of the more sensible
      // \n. Why? Because \n is treated as a new Block instead of a soft newline
      // within a paragraph!
      ui_ic_chatlog->textCursor().insertHtml("<br>");
    else
      ui_ic_chatlog->textCursor().insertText(": ", normal);
    // Format the result in italics
    ui_ic_chatlog->textCursor().insertText(p_text + ".", italics);
  }
  else {
    if (log_newline)
      // For some reason, we're forced to use <br> instead of the more sensible
      // \n. Why? Because \n is treated as a new Block instead of a soft newline
      // within a paragraph!
      ui_ic_chatlog->textCursor().insertHtml("<br>");
    else
      ui_ic_chatlog->textCursor().insertText(": ", normal);
    // Format the result according to html
    if (log_colors) {
      m_filter_base_stock = m_log_statement;
      QString p_text_filtered = filter_ic_text(p_text, true, -1, color);
      m_filter_base_stock = false;
      p_text_filtered = p_text_filtered.replace("$c0", chatlog_color.name(QColor::HexArgb));
      p_text_filtered = p_text_filtered.replace("$s0", chatlog_color.name(QColor::HexArgb));
      for (int c = 1; c < max_colors; ++c) {
        QColor color_result = m_log_palette.value(c, default_color_rgb_list.at(c));
        if (ghost) {
          color_result.setAlpha(128);
        }
        p_text_filtered = p_text_filtered.replace("$c" + QString::number(c), color_result.name(QColor::HexArgb));
        QColor stock_result = default_color_rgb_list.at(c);
        if (ghost)
          stock_result.setAlpha(128);
        p_text_filtered = p_text_filtered.replace("$s" + QString::number(c), stock_result.name(QColor::HexArgb));
      }
      ui_ic_chatlog->textCursor().insertHtml(p_text_filtered);
    }
    else
      ui_ic_chatlog->textCursor().insertText(filter_ic_text(p_text, false), normal);
  }

  // Only append with newline if log goes upwards
  if (!log_goes_downwards && need_newline) {
    ui_ic_chatlog->textCursor().insertBlock(format);
  }

  // If we got too many blocks in the current log, delete some.
  while (ui_ic_chatlog->document()->blockCount() > log_maximum_blocks &&
         log_maximum_blocks > 0) {
    QTextCursor temp_curs = ui_ic_chatlog->textCursor();
    temp_curs.movePosition(log_goes_downwards ? QTextCursor::Start
                                                 : QTextCursor::End);
    temp_curs.select(QTextCursor::BlockUnderCursor);
    temp_curs.removeSelectedText();
    if (log_goes_downwards)
      temp_curs.deleteChar();
    else
      temp_curs.deletePreviousChar();
  }

  // Finally, scroll the scrollbar to the correct position.
  if (old_cursor.hasSelection() ||
      old_scrollbar_value != scrollbar_target_value) {
    // The user has selected text or scrolled away from the bottom: maintain
    // position.
    ui_ic_chatlog->setTextCursor(old_cursor);
    ui_ic_chatlog->verticalScrollBar()->setValue(old_scrollbar_value);
  }
  else {
    ui_ic_chatlog->verticalScrollBar()->setValue(
        log_goes_downwards ? ui_ic_chatlog->verticalScrollBar()->maximum() : 0);
  }
}

void Courtroom::pop_ic_ghost()
{
  QTextCursor ghost = ui_ic_chatlog->textCursor();
  ghost.movePosition(log_goes_downwards ? QTextCursor::End
                                               : QTextCursor::Start);
  for (int i = 1; i < ghost_blocks; ++i) {
    ghost.movePosition(log_goes_downwards ? QTextCursor::PreviousBlock
                                                 : QTextCursor::NextBlock);
  }
  ghost.select(QTextCursor::BlockUnderCursor);
  ghost.removeSelectedText();
  if (ghost_blocks <= 1 && !log_goes_downwards)
    ghost.deleteChar();
  ghost_blocks--;
}

void Courtroom::play_preanim(bool immediate)
{
  QString f_char = m_chatmessage[CHAR_NAME];
  QString f_preanim = m_chatmessage[PRE_EMOTE];
  // all time values in char.inis are multiplied by a constant(time_mod) to get
  // the actual time
  int preanim_duration = ao_app->get_preanim_duration(f_char, f_preanim);
  int stay_time = ao_app->get_text_delay(f_char, f_preanim) * time_mod;
  int sfx_delay = m_chatmessage[SFX_DELAY].toInt() * time_mod;

  sfx_delay_timer->start(sfx_delay);
  QString anim_to_find =
      ao_app->get_image_suffix(ao_app->get_character_path(f_char, f_preanim));
  if (!file_exists(anim_to_find)) {
    if (immediate)
      anim_state = 4;
    else
      anim_state = 1;
    preanim_done();
    qWarning() << "could not find preanim" << f_preanim << "for character" << f_char;
    return;
  }
  ui_vp_player_char->set_static_duration(preanim_duration);
  ui_vp_player_char->set_play_once(true);
  ui_vp_player_char->load_image(f_preanim, f_char, preanim_duration, true);

  switch (m_chatmessage[DESK_MOD].toInt()) {
    case DESK_EMOTE_ONLY_EX:
      ui_vp_sideplayer_char->hide();
      ui_vp_sideplayer2_char->hide();
      ui_vp_player_char->move_and_center(0, 0);
      [[fallthrough]];
    case DESK_EMOTE_ONLY:
    case DESK_HIDE:
      set_scene(false, m_chatmessage[SIDE]);
      break;

    case DESK_PRE_ONLY_EX:
    case DESK_PRE_ONLY:
    case DESK_SHOW:
      set_scene(true, m_chatmessage[SIDE]);
      break;
  }

  if (immediate) {
    anim_state = 4;
    handle_ic_speaking();
  }
  else {
    anim_state = 1;
    if (stay_time >= 0)
      text_delay_timer->start(stay_time);
  }
}

void Courtroom::preanim_done()
{
  // Currently, someone's talking over us mid-preanim...
  if (anim_state != 1 && anim_state != 4 && anim_state != 5)
    return;
  anim_state = 1;

  handle_ic_speaking();
}

void Courtroom::start_chat_ticking()
{
  text_delay_timer->stop();
  // we need to ensure that the text isn't already ticking because this function
  // can be called by two logic paths
  if (text_state != 0)
    return;

  // The character shows right away, the text waits for the sign
  if (m_wtce_busy) {
    m_wtce_resume = true;
    return;
  }

  // Display the evidence
  display_evidence_image();

  // handle expanded desk mods
  switch(m_chatmessage[DESK_MOD].toInt()) {
    case DESK_EMOTE_ONLY_EX:
      set_self_offset(m_chatmessage[SELF_OFFSET]);
      [[fallthrough]];
    case DESK_EMOTE_ONLY:
    case DESK_SHOW:
      set_scene(true, m_chatmessage[SIDE]);
      break;

    case DESK_PRE_ONLY_EX:
      ui_vp_sideplayer_char->hide();
      ui_vp_sideplayer2_char->hide();
      ui_vp_player_char->move_and_center(0, 0);
      [[fallthrough]];
    case DESK_PRE_ONLY:
    case DESK_HIDE:
      set_scene(false, m_chatmessage[SIDE]);
      break;
  }

  if (m_chatmessage[EFFECTS] != "") {
    QStringList fx_list = m_chatmessage[EFFECTS].split("|");
    QString fx = fx_list[0];
    QString fx_sound;
    QString fx_folder;

    if (fx_list.length() > 1)
      fx_sound = fx_list[1];

    if (fx_list.length() > 2) {
      fx_folder = fx_list[1];
      fx_sound = fx_list[2];
    }
    this->do_effect(fx, fx_sound, m_chatmessage[CHAR_NAME], fx_folder);
  }
  else if (m_chatmessage[REALIZATION] == "1") {
    this->do_flash();
    sfx_player->play(ao_app->get_custom_realization(m_chatmessage[CHAR_NAME]));
  }
  int emote_mod = m_chatmessage[EMOTE_MOD].toInt(); // text meme bonanza
  if ((emote_mod == IDLE || emote_mod == ZOOM) && m_chatmessage[SCREENSHAKE] == "1") {
    this->do_screenshake();
  }
  if (m_chatmessage[MESSAGE].isEmpty()) {
    // since the message is empty, it's technically done ticking
    text_state = 2;
    if (m_charfx_out_pending) // ULTA: a blank line still fades its speaker out
      start_char_fx_out();
    if (m_chatmessage[ADDITIVE] == "1") {
      // Cool behavior
      ui_vp_chatbox->show();
      ui_vp_message->show();
    }
    else {
      ui_vp_chatbox->setVisible(chatbox_always_show);
      ui_vp_message->hide();
      // Show it if chatbox always shows
      if (Options::getInstance().characterStickerEnabled() && chatbox_always_show)
        ui_vp_sticker->load_image(m_chatmessage[CHAR_NAME]);
      // Hide the face sticker
      else {
        ui_vp_sticker->stop();
      }
    }
    // If we're not already waiting on the next message, start the timer. We could be overriden if
    // there's an objection planned.
    int delay = Options::getInstance().textStayTime();
    if (delay > 0 && !text_queue_timer->isActive())
      text_queue_timer->start(delay);
    return;
  }

  ui_vp_chatbox->show();
  ui_vp_message->show();

  if (Options::getInstance().characterStickerEnabled())
    ui_vp_sticker->load_image(m_chatmessage[CHAR_NAME]);

  if (m_chatmessage[ADDITIVE] != "1") {
    ui_vp_message->clear();
    real_tick_pos = 0;
    additive_previous = "";
  }

  tick_pos = 0;
  blip_ticker = 0;
  text_crawl = Options::getInstance().textCrawlSpeed();
  blip_rate = Options::getInstance().blipRate();
  blank_blip = Options::getInstance().blankBlip();

  // At the start of every new message, we set the text speed to the default.
  current_display_speed = 3;
  chat_tick_timer->start(0); // Display the first char right away

  last_misc = current_misc;
  current_misc = ao_app->get_chat(style_char());
  if (last_misc != current_misc || char_color_rgb_list.size() < max_colors)
    gen_char_rgb_list(current_misc);

  QString f_blips = ao_app->get_blips(style_char());
  blip_player->set_blips(f_blips);

  // means text is currently ticking
  text_state = 1;

  c_played = false;
}

void Courtroom::chat_tick()
{
  // note: this is called fairly often
  // do not perform heavy operations here

  QString f_message = m_chatmessage[MESSAGE];

  // Due to our new text speed system, we always need to stop the timer now.
  chat_tick_timer->stop();
  ui_vp_player_char->set_static_duration(0);
  QString filename;

  if (tick_pos >= f_message.size()) {
    text_state = 2;
    if (m_charfx_out_pending)
      start_char_fx_out();
    // Check if we're a narrator msg
    if (m_chatmessage[EMOTE] != "") {
      if (anim_state < 3) {
        QStringList c_paths = {
          ao_app->get_image_suffix(ao_app->get_character_path(m_chatmessage[CHAR_NAME], "(c)" + m_chatmessage[EMOTE])),
          ao_app->get_image_suffix(ao_app->get_character_path(m_chatmessage[CHAR_NAME], "(c)/" + m_chatmessage[EMOTE]))
          };
        // if there is a (c) animation for this emote and we haven't played it already
        if (file_exists(ui_vp_player_char->find_image(c_paths)) &&(!c_played)) {
          anim_state = 5;
          ui_vp_player_char->set_play_once(true);
          filename = "(c)" + m_chatmessage[EMOTE];
          c_played = true;
        }
        else {
          anim_state = 3;
          ui_vp_player_char->set_play_once(false);
          filename = "(a)" + m_chatmessage[EMOTE];
        }
        ui_vp_player_char->load_image(filename, m_chatmessage[CHAR_NAME], 0,
                                        false);
      }
    }
    else // We're a narrator msg
      anim_state = 3;
    QString f_char;
    QString f_custom_theme;
    if (Options::getInstance().customChatboxEnabled()) {
      f_char = style_char();
      f_custom_theme = ao_app->get_chat(f_char);
    }
    ui_vp_chat_arrow->transform_mode = ao_app->get_misc_scaling(f_custom_theme);
    ui_vp_chat_arrow->load_image("chat_arrow", f_custom_theme); // Chat stopped being processed, indicate that.
    m_filter_base_stock = m_msg_statement;
    QString f_message_filtered = filter_ic_text(f_message, true, -1, m_chatmessage[TEXT_COLOR].toInt());
    m_filter_base_stock = false;
    for (int c = 0; c < max_colors; ++c) {
      additive_previous = additive_previous.replace("$c" + QString::number(c), m_msg_palette.value(c, char_color_rgb_list.at(c)).name(QColor::HexRgb));
      additive_previous = additive_previous.replace("$s" + QString::number(c), char_color_rgb_list.at(c).name(QColor::HexRgb));
      f_message_filtered = f_message_filtered.replace("$c" + QString::number(c), m_msg_palette.value(c, char_color_rgb_list.at(c)).name(QColor::HexRgb));
      f_message_filtered = f_message_filtered.replace("$s" + QString::number(c), char_color_rgb_list.at(c).name(QColor::HexRgb));
    }
    additive_previous = additive_previous + f_message_filtered;
    real_tick_pos = ui_vp_message->toPlainText().size();


    // If we're not already waiting on the next message, start the timer. We could be overriden if
    // there's an objection planned.
    int delay = Options::getInstance().textStayTime();
    if (delay > 0 && !text_queue_timer->isActive())
      text_queue_timer->start(delay);

    // if we have instant objections disabled, and queue is not empty, check if next message after this
    // is an objection.
    if (!Options::getInstance().objectionSkipQueueEnabled() && chatmessage_queue.size() > 0)
    {
      QStringList p_contents = chatmessage_queue.head();
      int objection_mod = p_contents[OBJECTION_MOD].split("&")[0].toInt();
      bool is_objection = objection_mod >= 1 && objection_mod <= 5;
      // If this is an objection, we'll need to interrupt our current message.
      if (is_objection)
        chatmessage_dequeue();
    }
    return;
  }

  // Stops blips from playing when we have a formatting option.
  bool formatting_char = false;

  QString f_rest = f_message;

  // Alignment characters
  if (tick_pos < 2) {
    if (f_rest.startsWith("~~") || f_rest.startsWith("~>") || f_rest.startsWith("<>"))
      tick_pos += 2;
  }
  f_rest.remove(0, tick_pos);
  QTextBoundaryFinder tbf(QTextBoundaryFinder::Grapheme, f_rest);
  QString f_character;
  int f_char_length;
  int msg_delay = 0;

  tbf.toNextBoundary();

  if (tbf.position() == -1)
    f_character = f_rest;
  else
    f_character = f_rest.left(tbf.position());

  f_char_length = f_character.length();
  tick_pos += f_char_length;

  // Escape character.
  if (!next_character_is_not_special) {
    if (f_character == "\\") {
      next_character_is_not_special = true;
      formatting_char = true;
    }

    // Text speed modifier.
    else if (f_character == "{") {
      // ++, because it INCREASES delay!
      current_display_speed++;
      formatting_char = true;
    }
    else if (f_character == "}") {
      current_display_speed--;
      formatting_char = true;
    }

    else {
      // Parse markdown colors
      for (int c = 0; c < max_colors; ++c) {
        QString markdown_start = color_markdown_start_list.at(c);
        QString markdown_end = color_markdown_end_list.at(c);
        bool markdown_remove = color_markdown_remove_list.at(c);
        if (markdown_start.isEmpty())
          continue;

        if (f_character == markdown_start || f_character == markdown_end) {
          if (markdown_remove)
            formatting_char = true;
          break;
        }
      }
    }
  }
  else {
    if (f_character == "n")
      formatting_char = true; // it's a newline
    if (f_character == "s")   // Screenshake.
    {
      this->do_screenshake();
      formatting_char = true;
    }
    if (f_character == "f") // Flash.
    {
      this->do_flash();
      formatting_char = true;
    }
    if (f_character == "p")
    {
      formatting_char = true;
    }
    next_character_is_not_special = false;
  }

  // Keep the speed at bay.
  if (current_display_speed < 0)
    current_display_speed = 0;
  else if (current_display_speed > 6)
    current_display_speed = 6;

  if (msg_delay == 0)
    msg_delay = text_crawl * message_display_mult[current_display_speed];

  if ((msg_delay <= 0 &&
       tick_pos < f_message.size() - 1) ||
      formatting_char) {
    if (f_character == "p")
      chat_tick_timer->start(100); // wait the pause lol
    else
      chat_tick_timer->start(0); // Don't bother rendering anything out as we're
                                 // doing the SPEED. (there's latency otherwise)
    if (!formatting_char || f_character == "n" || f_character == "f" ||
        f_character == "s" || f_character == "p")
      real_tick_pos += f_char_length; // Adjust the tick position for the
                                      // scrollbar convenience
  }
  else {
    // Do the colors, gradual showing, etc. in here
    m_filter_base_stock = m_msg_statement;
    QString f_message_filtered = filter_ic_text(f_message, true, tick_pos, m_chatmessage[TEXT_COLOR].toInt());
    m_filter_base_stock = false;
    for (int c = 0; c < max_colors; ++c) {
      additive_previous = additive_previous.replace("$c" + QString::number(c), m_msg_palette.value(c, char_color_rgb_list.at(c)).name(QColor::HexRgb));
      additive_previous = additive_previous.replace("$s" + QString::number(c), char_color_rgb_list.at(c).name(QColor::HexRgb));
      f_message_filtered = f_message_filtered.replace("$c" + QString::number(c), m_msg_palette.value(c, char_color_rgb_list.at(c)).name(QColor::HexRgb));
      f_message_filtered = f_message_filtered.replace("$s" + QString::number(c), char_color_rgb_list.at(c).name(QColor::HexRgb));
    }
    ui_vp_message->setHtml(additive_previous + f_message_filtered);

    // This should always be done AFTER setHtml. Scroll the chat window with the
    // text.

    // Make the cursor follow the message
    QTextCursor cursor = ui_vp_message->textCursor();
    cursor.setPosition(real_tick_pos);
    ui_vp_message->setTextCursor(cursor);
    real_tick_pos += f_char_length;

    ui_vp_message->ensureCursorVisible();

    // We blip every "blip rate" letters.
    // Here's an example with blank_blip being false and blip_rate being 2:
    // I am you
    // ! !  ! !
    // where ! is the blip sound
    int b_rate = blip_rate;
    // Overwhelming blip spam prevention, this method is more consistent than timers
    if (msg_delay != 0 && msg_delay <= 25) {
      // The default blip speed is 40ms, and if current msg_delay is 25ms,
      // the formula will result in the blip rate of:
      // 40/25 = 1.6 = 2
      // And if it's faster than that:
      // 40/10 = 4
      b_rate =
          qMax(b_rate, qRound(static_cast<float>(text_crawl) /
                              msg_delay));
    }
    if ((blip_rate <= 0 && blip_ticker < 1) || (b_rate > 0 && blip_ticker % b_rate == 0)) {
      // ignoring white space unless blank_blip is enabled.
      if (!formatting_char && (f_character != ' ' || blank_blip)) {
        blip_player->blip_tick();
        ++blip_ticker;
      }
    }
    else {
      // Don't fully ignore whitespace still, keep ticking until
      // we reached the need to play a blip sound - we also just
      // need to wait for a letter to play it on.
      ++blip_ticker;
    }

    // Punctuation delayer, only kicks in on speed ticks less than }}
    if (current_display_speed > 1 && punctuation_chars.contains(f_character)) {
      // Making the user have to wait any longer than 1.5 of the slowest speed
      // is downright unreasonable
      int max_delay = text_crawl * message_display_mult[6] * 1.5;
      msg_delay = qMin(max_delay, msg_delay * punctuation_modifier);
    }

    if (m_chatmessage[EMOTE] != "") {
      // If this color is talking
      if (color_is_talking && anim_state != 2 &&
          anim_state <
          4) // Set it to talking as we're not on that already (though we have
        // to avoid interrupting a non-interrupted preanim)
      {
        ui_vp_player_char->stop();
        ui_vp_player_char->set_play_once(false);
        filename = "(b)" + m_chatmessage[EMOTE];
        ui_vp_player_char->load_image(filename, m_chatmessage[CHAR_NAME], 0,
                                      false);
        anim_state = 2;
      }
      else if (!color_is_talking && anim_state < 3 &&
               anim_state != 3) // Set it to idle as we're not on that already
      {
        ui_vp_player_char->stop();
        ui_vp_player_char->set_play_once(false);
        filename = "(a)" + m_chatmessage[EMOTE];
        ui_vp_player_char->load_image(filename, m_chatmessage[CHAR_NAME], 0,
                                      false);
        anim_state = 3;
      }
    }
    // Continue ticking
    chat_tick_timer->start(msg_delay);
  }
}

void Courtroom::play_sfx()
{
  QString sfx_name = m_chatmessage[SFX_NAME];
  if (m_chatmessage[SCREENSHAKE] ==
      "1") // Screenshake dependant on preanim sfx delay meme
  {
    this->do_screenshake();
  }
  if (sfx_name == "1")
    return;

  sfx_player->play(sfx_name);
  if (Options::getInstance().loopingSfx())
    sfx_player->set_looping(
        ao_app->get_sfx_looping(current_char, current_emote) == "1");
}

void Courtroom::set_scene(bool show_desk, const QString f_side)
{
  ui_vp_background->load_image(ao_app->get_pos_path(f_side));
  ui_vp_desk->load_image(ao_app->get_pos_path(f_side, true));

  if (show_desk)
    ui_vp_desk->show();
  else
    ui_vp_desk->hide();
}

void Courtroom::set_self_offset(const QString& p_list) {
    QStringList self_offsets = p_list.split("&");
    int self_offset = self_offsets[0].toInt();
    int self_offset_v;
    if (self_offsets.length() <= 1) {
      self_offset_v = 0;
    }
    else {
      self_offset_v = self_offsets[1].toInt();
    }
    ui_vp_player_char->move_and_center(ui_viewport->width() * self_offset / 100,
                                       ui_viewport->height() * self_offset_v / 100);
}

void Courtroom::set_ip_list(QString p_list)
{
  QString f_list = p_list.replace("|", ":").replace("*", "\n");

  ui_server_chatlog->append(f_list);
}

void Courtroom::set_mute(bool p_muted, int p_cid)
{
  if (p_cid != m_cid && p_cid != -1)
    return;

  if (p_muted)
    ui_muted->show();
  else {
    ui_muted->hide();
    ui_ic_chat_message->setFocus();
  }

  ui_muted->resize(ui_ic_chat_message->width(), ui_ic_chat_message->height());
  ui_muted->set_image("muted");

  is_muted = p_muted;
  ui_ic_chat_message->setEnabled(!p_muted);
}

void Courtroom::set_ban(int p_cid)
{
  if (p_cid != m_cid && p_cid != -1)
    return;

  call_notice(tr("You have been banned."));

  ao_app->construct_lobby();
  ao_app->destruct_courtroom();
}

void Courtroom::handle_song(QStringList *p_contents)
{
  QStringList f_contents = *p_contents;

  if (f_contents.size() < 2)
    return;

  bool ok; // Used for charID, channel, effect check
  bool looping = false; // No loop due to outdated server using serverside looping
  int channel = 0; // Channel 0 is 'master music', other for ambient
  int effect_flags = 0; // No effects by default - vanilla functionality

  QString f_song = f_contents.at(0);
  const QString song_key = f_song; // as named in the music list
  QString f_song_clear = QUrl(f_song).fileName();
  f_song_clear = f_song_clear.left(f_song_clear.lastIndexOf('.'));

  int n_char = f_contents.at(1).toInt(&ok);
  if (!ok)
    return;

  if (p_contents->length() > 3 && p_contents->at(3) == "1")
    looping = true;

  if (p_contents->length() > 4) {
    // eyyy we want to change this song's CHANNEL huh
    // let the music player handle it if it's bigger than the channel list
    channel = p_contents->at(4).toInt(&ok);
    if (!ok)
      return;
  }
  if (p_contents->length() > 5) {
    // Flags provided to us by server such as Fade In, Fade Out, Sync Pos etc.
    effect_flags = p_contents->at(5).toInt(&ok);
    if (!ok)
      return;
  }

  if(!file_exists(ao_app->get_sfx_suffix(ao_app->get_music_path(f_song))) && !f_song.startsWith("http")
          && f_song != "~stop.mp3" && !ao_app->asset_url.isEmpty()) {
      f_song = (ao_app->asset_url + "sounds/music/" + f_song).toLower();
  }

  bool is_stop = (f_song == "~stop.mp3");
  if (n_char >= 0 && n_char < char_list.size()) {
    QString str_char = char_list.at(n_char).name;
    QString str_show = ao_app->get_showname(str_char);
    if (p_contents->length() > 2) {
      if (p_contents->at(2) != "") {
        str_show = p_contents->at(2);
      }
    }
    if (!mute_map.value(n_char)) {
      bool selfname = n_char == m_cid;
      if (is_stop) {
        log_ic_text(str_char, str_show, "", tr("has stopped the music"), 0, selfname);
        append_ic_text("", str_show, tr("has stopped the music"), 0, selfname);
      }
      else {
        log_ic_text(str_char, str_show, f_song, tr("has played a song"), 0, selfname);
        append_ic_text(f_song_clear, str_show, tr("has played a song"), 0, selfname);
      }
    }
  }

  if (channel == 0) {
    // Current song UI only displays the song playing, not other channels.
    // Any other music playing is irrelevant.
    if (music_player->music_watcher.isRunning()) {
        music_player->music_watcher.cancel();
    }
    ui_music_name->setText(tr("[LOADING] %1").arg(f_song_clear));
    m_playing_song = is_stop ? QString() : song_key;
    update_playing_song_highlight();
  }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  music_player->music_watcher.setFuture(QtConcurrent::run(music_player, &AOMusicPlayer::play, f_song, channel,
                                                          looping, effect_flags));
#else
  music_player->music_watcher.setFuture(QtConcurrent::run(&AOMusicPlayer::play, music_player, f_song, channel,
                                                          looping, effect_flags));
#endif
}

void Courtroom::update_ui_music_name()
{
    QString result = music_player->music_watcher.result();
    if (result.isEmpty())
      return;
    ui_music_name->setText(result);
}

void Courtroom::wtce_finished()
{
  if (!m_wtce_busy)
    return;
  m_wtce_busy = false;
  m_wtce_timer->stop();
  if (m_wtce_resume) {
    m_wtce_resume = false;
    start_chat_ticking();
  }
}

void Courtroom::handle_wtce(QString p_wtce, int variant)
{
  //QString sfx_file = "courtroom_sounds.ini";
  QString bg_misc = ao_app->read_design_ini("misc", ao_app->get_background_path("design.ini"));
  QString sfx_name;
  QString filename;
  ui_vp_wtce->set_static_duration(wtce_static_time);
  ui_vp_wtce->set_max_duration(wtce_max_time);
  // witness testimony
  if (p_wtce == "testimony1") {
    // End testimony indicator
    if (variant == 1) {
      ui_vp_testimony->kill();
      return;
    }
    sfx_name = ao_app->get_court_sfx("witness_testimony", bg_misc);
    if (sfx_name == "") {
      sfx_name = ao_app->get_court_sfx("witnesstestimony", bg_misc);
    }
    filename = "witnesstestimony_bubble";
    ui_vp_testimony->load_image("testimony", "", bg_misc);
  }
  // cross examination
  else if (p_wtce == "testimony2") {
      sfx_name = ao_app->get_court_sfx("cross_examination", bg_misc);
    if (sfx_name == "") {
      sfx_name = ao_app->get_court_sfx("crossexamination", bg_misc);
    }
    filename = "crossexamination_bubble";
    ui_vp_testimony->kill();
  }
  else {
    ui_vp_wtce->set_static_duration(verdict_static_time);
    ui_vp_wtce->set_max_duration(verdict_max_time);
    // Verdict?
    if (p_wtce == "judgeruling") {
      if (variant == 0) {
        sfx_name = ao_app->get_court_sfx("not_guilty", bg_misc);
        if (sfx_name == "") {
          sfx_name = ao_app->get_court_sfx("notguilty", bg_misc);
        }
        filename = "notguilty_bubble";
        ui_vp_testimony->kill();
      }
      else if (variant == 1) {
        sfx_name = ao_app->get_court_sfx("guilty", bg_misc);
        filename = "guilty_bubble";
        ui_vp_testimony->kill();
      }
    }
    // Completely custom WTCE
    else {
      sfx_name = p_wtce;
      filename = p_wtce;
    }
  }
  sfx_player->play(sfx_name);
  ui_vp_wtce->load_image(filename, "", bg_misc);
  ui_vp_wtce->set_play_once(true);

  // Text waits until the testimony or cross examination sign is done
  if (p_wtce == "testimony2" || (p_wtce == "testimony1" && variant != 1)) {
    m_wtce_busy = true;
    m_wtce_timer->start(wtce_max_time + 400);
  }
}

void Courtroom::set_hp_bar(int p_bar, int p_state)
{
  if (p_state < 0 || p_state > 10)
    return;

  int prev_state = p_state;
  if (p_bar == 1) {
    ui_defense_bar->set_image("defensebar" + QString::number(p_state));
    prev_state = defense_bar_state;
    defense_bar_state = p_state;
  }
  else if (p_bar == 2) {
    ui_prosecution_bar->set_image("prosecutionbar" + QString::number(p_state));
    prev_state = prosecution_bar_state;
    prosecution_bar_state = p_state;
  }

  QString sfx_name;
  QString effect_name;
  if (p_state > prev_state) {
    sfx_name = ao_app->get_penalty_value("hp_increased_sfx");
    effect_name = ao_app->get_penalty_value("hp_increased_effect").toLower();
  }
  else if (p_state < prev_state) {
    sfx_name = ao_app->get_penalty_value("hp_decreased_sfx");
    effect_name = ao_app->get_penalty_value("hp_decreased_effect").toLower();
  }
  else {
    return;
  }

  if (effect_name == "screenshake") {
    do_screenshake();
  }
  else if (effect_name == "flash") {
    do_flash();
  }
  else {
    do_effect(effect_name, "", "", "");
  }

  if (!sfx_name.isEmpty()) {
    sfx_player->play(sfx_name);
  }
}

void Courtroom::show_judge_controls(bool visible)
{
  if (judge_state != POS_DEPENDENT) {
    visible = judge_state == SHOW_CONTROLS; // Server-side override
  }
  QList<QWidget*> judge_controls =
      {
        ui_witness_testimony,
        ui_cross_examination,
        ui_guilty,
        ui_not_guilty,
        ui_defense_minus,
        ui_defense_plus,
        ui_prosecution_minus,
        ui_prosecution_plus
      };

  for (QWidget* control : judge_controls) {
      if (visible)
          control->show();
      else
          control->hide();
  }
}

void Courtroom::mod_called(QString p_ip)
{
  ui_server_chatlog->append(p_ip);
  if (!ui_guard->isChecked()) {
    modcall_player->play(ao_app->get_court_sfx("mod_call"));
    ao_app->alert(this);
  }
}

void Courtroom::on_ooc_return_pressed()
{
  QString ooc_message = ui_ooc_chat_message->text();

  //We ignore it when the server is compatible with 2.8
  //Using an arbitrary 2.8 feature flag certainly won't cause issues someday.
  if (ooc_message.startsWith("/pos") & !ao_app->effects_supported) {
    if (ooc_message == "/pos jud") {
      show_judge_controls(true);
    }
    else {
      show_judge_controls(false);
    }
  }

  if (ooc_message.startsWith("/load_case")) {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    QStringList command = ooc_message.split(" ", QString::SkipEmptyParts);
#else
    QStringList command = ooc_message.split(" ", Qt::SkipEmptyParts);
#endif
    QDir casefolder(get_base_path()+"/cases");
    if (!casefolder.exists()) {
      QDir::current().mkdir(get_base_path() + casefolder.dirName());
      append_server_chatmessage(
          "CLIENT",
          tr("You don't have a `base/cases/` folder! It was just made for you, "
             "but seeing as it WAS just made for you, it's likely the case "
             "file you're looking for can't be found in there."),
          "1");
      ui_ooc_chat_message->clear();
      return;
    }
    QStringList caseslist = casefolder.entryList();
    caseslist.removeOne(".");
    caseslist.removeOne("..");
    caseslist.replaceInStrings(".ini", "");

    if (command.size() < 2) {
      append_server_chatmessage(
          "CLIENT",
          tr("You need to give a filename to load (extension not needed)! Make "
             "sure that it is in the `base/cases/` folder, and that it is a "
             "correctly formatted ini.\nCases you can load: %1")
              .arg(caseslist.join(", ")),
          "1");
      ui_ooc_chat_message->clear();
      return;
    }

    if (command.size() > 2) {
      append_server_chatmessage(
          "CLIENT",
          tr("Too many arguments to load a case! You only need one filename, "
             "without extension."),
          "1");
      ui_ooc_chat_message->clear();
      return;
    }

    QSettings casefile(get_base_path() + "/cases/" + command[1] + ".ini",
                       QSettings::IniFormat);

    QString caseauth = casefile.value("author", "").value<QString>();
    QString casedoc = casefile.value("doc", "").value<QString>();
    QString cmdoc = casefile.value("cmdoc", "").value<QString>();
    QString casestatus = casefile.value("status", "").value<QString>();

    if (!caseauth.isEmpty())
      append_server_chatmessage(tr("CLIENT"),
                                tr("Case made by %1.").arg(caseauth), "1");
    if (!casedoc.isEmpty()) {
      ao_app->send_server_packet(new AOPacket("CT", {ui_ooc_chat_name->text(), "/doc " + casedoc}));
    }
    if (!casestatus.isEmpty()) {
      ao_app->send_server_packet(new AOPacket("CT", {ui_ooc_chat_name->text(), "/status " + casestatus}));
    }
    if (!cmdoc.isEmpty())
      append_server_chatmessage(
          "CLIENT", tr("Navigate to %1 for the CM doc.").arg(cmdoc), "1");

    for (int i = local_evidence_list.size() - 1; i >= 0; i--) {
      ao_app->send_server_packet(
          new AOPacket("DE", {QString::number(i)}));
    }

    // sort the case_evidence numerically
    QStringList case_evidence = casefile.childGroups();
    std::sort(case_evidence.begin(), case_evidence.end(),
              [] (const QString &a, const QString &b) {
                return a.toInt() < b.toInt();
              });

    // load evidence
    foreach (QString evi, case_evidence) {
      if (evi == "General")
        continue;

      QStringList f_contents;

      f_contents.append(
          casefile.value(evi + "/name", tr("UNKNOWN")).value<QString>());
      f_contents.append(
          casefile.value(evi + "/description", tr("UNKNOWN")).value<QString>());
      f_contents.append(
          casefile.value(evi + "/image", "UNKNOWN.png").value<QString>());

      ao_app->send_server_packet(new AOPacket("PE", f_contents));
    }

    append_server_chatmessage(
        "CLIENT", tr("Your case \"%1\" was loaded!").arg(command[1]), "1");
    ui_ooc_chat_message->clear();
    return;
  }
  else if (ooc_message.startsWith("/save_case")) {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    QStringList command = ooc_message.split(" ", QString::SkipEmptyParts);
#else
    QStringList command = ooc_message.split(" ", Qt::SkipEmptyParts);
#endif
    QDir casefolder(get_base_path() + "cases");
    if (!casefolder.exists()) {
      QDir(get_base_path()).mkdir(casefolder.dirName());
      append_server_chatmessage(
          "CLIENT",
          tr("You don't have a `base/cases/` folder! It was just made for you, "
             "but seeing as it WAS just made for you, it's likely that you "
             "somehow deleted it."),
          "1");
      ui_ooc_chat_message->clear();
      return;
    }
    QStringList caseslist = casefolder.entryList();
    caseslist.removeOne(".");
    caseslist.removeOne("..");
    caseslist.replaceInStrings(".ini", "");

    if (command.size() < 3) {
      append_server_chatmessage(
          "CLIENT",
          tr("You need to give a filename to save (extension not needed) and "
             "the courtroom status!"),
          "1");
      ui_ooc_chat_message->clear();
      return;
    }

    if (command.size() > 3) {
      append_server_chatmessage(
          "CLIENT",
          tr("Too many arguments to save a case! You only need a filename "
             "without extension and the courtroom status!"),
          "1");
      ui_ooc_chat_message->clear();
      return;
    }
    QSettings casefile(get_base_path() + "/cases/" + command[1] + ".ini",
                       QSettings::IniFormat);
    casefile.setValue("author", ui_ooc_chat_name->text());
    casefile.setValue("cmdoc", "");
    casefile.setValue("doc", "");
    casefile.setValue("status", command[2]);
    casefile.sync();
    for (int i = 0; i < local_evidence_list.size(); i++) {
      QString clean_evidence_dsc = local_evidence_list[i].description.replace(
          QRegularExpression("<owner = ...>..."), "");
      clean_evidence_dsc = clean_evidence_dsc.replace(
          clean_evidence_dsc.lastIndexOf(">"), 1, "");
      casefile.beginGroup(QString::number(i));
      casefile.sync();
      casefile.setValue("name", local_evidence_list[i].name);
      casefile.setValue("description", local_evidence_list[i].description);
      casefile.setValue("image", local_evidence_list[i].image);
      casefile.endGroup();
    }
    casefile.sync();
    append_server_chatmessage(
        "CLIENT", tr("Succesfully saved, edit doc and cmdoc link on the ini!"),
        "1");
    ui_ooc_chat_message->clear();
    return;
  }

  QStringList packet_contents;
  packet_contents.append(ui_ooc_chat_name->text());
  packet_contents.append(ooc_message);

  AOPacket *f_packet = new AOPacket("CT", packet_contents);

  if (server_ooc)
    ao_app->send_server_packet(f_packet);

  ui_ooc_chat_message->clear();

  ui_ooc_chat_message->setFocus();
}

void Courtroom::on_ooc_toggle_clicked()
{
  if (server_ooc) {
    ui_debug_log->show();
    ui_server_chatlog->hide();
    ui_ooc_toggle->setText(tr("Debug"));

    server_ooc = false;
  }
  else {
    ui_debug_log->hide();
    ui_server_chatlog->show();
    ui_ooc_toggle->setText(tr("Server"));

    server_ooc = true;
  }
  sync_log_under_lists();
}

// Todo: multithread this due to some servers having large as hell music list
void Courtroom::on_music_search_edited(QString p_text)
{
  // Iterate through all QTreeWidgetItem items
  if (!ui_music_list->isHidden()) {
    QTreeWidgetItemIterator it(ui_music_list);
    while (*it) {
      (*it)->setHidden(p_text != "");
      ++it;
    }
    last_music_search = p_text;
  }

  if (!ui_area_list->isHidden()) {
    QTreeWidgetItemIterator ait(ui_area_list);
    while (*ait) {
      (*ait)->setHidden(p_text != "");
      ++ait;
    }
    last_area_search = p_text;
  }

  if (p_text != "") {
    if (!ui_music_list->isHidden()) {
      // Search in metadata
      QList<QTreeWidgetItem *> clist = ui_music_list->findItems(
          ui_music_search->text(), Qt::MatchContains | Qt::MatchRecursive, 1);
      foreach (QTreeWidgetItem *item, clist) {
        if (item->parent() != nullptr) // So the category shows up too
          item->parent()->setHidden(false);
        item->setHidden(false);
      }
    }

    if (!ui_area_list->isHidden()) {
      // Search in metadata
      QList<QTreeWidgetItem *> alist = ui_area_list->findItems(
          ui_music_search->text(), Qt::MatchContains | Qt::MatchRecursive, 1);
      foreach (QTreeWidgetItem *item, alist) {
        if (item->parent() != nullptr) // So the category shows up too
          item->parent()->setHidden(false);
        item->setHidden(false);
      }
    }
  }
}

void Courtroom::on_music_search_return_pressed()
{
  if (ui_music_search->text() == "") {
    ui_music_list->collapseAll();
  }
}

void Courtroom::on_pos_dropdown_changed(int p_index)
{
  if (p_index < 0)
    return;
  on_pos_dropdown_changed(ui_pos_dropdown->itemText(p_index));
}

void Courtroom::on_pos_dropdown_changed(QString p_text)
{
  set_side(p_text);
}

void Courtroom::on_pos_dropdown_context_menu_requested(const QPoint &pos)
{
  QMenu *menu = ui_iniswap_dropdown->lineEdit()->createStandardContextMenu();

  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->addSeparator();

  menu->addAction(QString("Open background " + current_background), this,
                  [=] {
    QString p_path = ao_app->get_real_path(VPath("background/" + current_background + "/"));
    if (!dir_exists(p_path)) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
  }
  );
  menu->popup(ui_iniswap_dropdown->mapToGlobal(pos));
}

void Courtroom::on_pos_remove_clicked()
{
  ui_pos_dropdown->blockSignals(true);
  QString default_side = ao_app->get_char_side(current_char);

  show_judge_controls(ao_app->get_pos_is_judge(default_side));

  for (int i = 0; i < ui_pos_dropdown->count(); ++i) {
    QString pos = ui_pos_dropdown->itemText(i);
    if (pos == default_side) {
      ui_pos_dropdown->setCurrentIndex(i);
      break;
    }
  }
  int wit_index = ui_pos_dropdown->findText("wit");
  if (ui_pos_dropdown->currentText() != default_side && wit_index != -1) //i.e. this bg doesn't have our pos
    ui_pos_dropdown->setCurrentIndex(wit_index); // fall back to "wit"
  else if (ui_pos_dropdown->currentText() != default_side) // we don't have "wit" either?
    ui_pos_dropdown->setCurrentIndex(0); // as a last resort, choose the first item in the dropdown
  ui_pos_dropdown->blockSignals(false);
  current_side = "";
  ui_pos_remove->hide();
  ui_ic_chat_message->setFocus();
}

void Courtroom::set_iniswap_dropdown()
{
  ui_iniswap_dropdown->blockSignals(true);
  ui_iniswap_dropdown->clear();
  if (m_cid == -1) {
    ui_iniswap_dropdown->hide();
    ui_iniswap_remove->hide();
    return;
  }
  QStringList iniswaps =
      ao_app->get_list_file(ao_app->get_character_path(char_list.at(m_cid).name, "iniswaps.ini")) +
      ao_app->get_list_file(VPath("iniswaps.ini"));

  iniswaps.prepend(char_list.at(m_cid).name);
  iniswaps.removeDuplicates();
  if (iniswaps.size() <= 0) {
    ui_iniswap_dropdown->hide();
    ui_iniswap_remove->hide();
    return;
  }
  ui_iniswap_dropdown->show();
  for (int i = 0; i < iniswaps.size(); ++i) {
    ui_iniswap_dropdown->addItem(iniswaps.at(i));
    QString icon_path = ao_app->get_image_suffix(ao_app->get_character_path(
                                                   iniswaps.at(i), "char_icon"));
    ui_iniswap_dropdown->setItemIcon(i, QIcon(icon_path));
    if (iniswaps.at(i) == current_char) {
      ui_iniswap_dropdown->setCurrentIndex(i);
      if (i != 0)
        ui_iniswap_remove->show();
      else
        ui_iniswap_remove->hide();
    }
  }
  ui_iniswap_dropdown->blockSignals(false);
}

void Courtroom::on_iniswap_dropdown_changed(int p_index)
{
  ui_ic_chat_message->setFocus();
  QString iniswap = ui_iniswap_dropdown->itemText(p_index);

  QStringList swaplist;
  QStringList defswaplist = ao_app->get_list_file(ao_app->get_character_path(char_list.at(m_cid).name, "iniswaps.ini"));
  for (int i = 0; i < ui_iniswap_dropdown->count(); ++i) {
    QString entry = ui_iniswap_dropdown->itemText(i);
    if (!swaplist.contains(entry) && entry != char_list.at(m_cid).name && !defswaplist.contains(entry))
      swaplist.append(entry);
  }
  QString p_path = ao_app->get_real_path(VPath("iniswaps.ini"));
  if (!file_exists(p_path)) {
    p_path = get_base_path() + "iniswaps.ini";
  }
  ao_app->write_to_file(swaplist.join("\n"), p_path);
  ui_iniswap_dropdown->blockSignals(true);
  ui_iniswap_dropdown->setCurrentIndex(p_index);
  ui_iniswap_dropdown->blockSignals(false);
  update_character(m_cid, iniswap, true);
  QString icon_path = ao_app->get_image_suffix(ao_app->get_character_path(
                                                 iniswap, "char_icon"));
  ui_iniswap_dropdown->setItemIcon(p_index, QIcon(icon_path));
  if (p_index != 0)
    ui_iniswap_remove->show();
  else
    ui_iniswap_remove->hide();
}

void Courtroom::on_iniswap_context_menu_requested(const QPoint &pos)
{
  QMenu *menu = ui_iniswap_dropdown->lineEdit()->createStandardContextMenu();

  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->addSeparator();
  if (file_exists(ao_app->get_real_path(
                    ao_app->get_character_path(current_char, "char.ini"))))
    menu->addAction(QString("Edit " + current_char + "/char.ini"), this,
                    &Courtroom::on_iniswap_edit_requested);
  if (ui_iniswap_dropdown->itemText(ui_iniswap_dropdown->currentIndex()) !=
      char_list.at(m_cid).name)
    menu->addAction(QString("Remove " + current_char), this,
                    &Courtroom::on_iniswap_remove_clicked);

  menu->addSeparator();
  menu->addAction(QString("Open character folder " + current_char), this,
                  [=] {
    QString p_path = ao_app->get_real_path(VPath("characters/" + current_char + "/"));
    if (!dir_exists(p_path)) {
      return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
  }
  );
  menu->popup(ui_iniswap_dropdown->mapToGlobal(pos));
}

void Courtroom::on_iniswap_edit_requested()
{
  QString p_path = ao_app->get_real_path(ao_app->get_character_path(current_char, "char.ini"));
  if (!file_exists(p_path)) {
    return;
  }
  QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
}

void Courtroom::on_iniswap_remove_clicked()
{
  if (ui_iniswap_dropdown->count() <= 0) {
    ui_iniswap_remove->hide(); // We're not supposed to see it. Do this or the
                               // client will crash
    return;
  }
  QStringList defswaplist = ao_app->get_list_file(ao_app->get_character_path(char_list.at(m_cid).name, "iniswaps.ini"));
  QString iniswap = ui_iniswap_dropdown->itemText(ui_iniswap_dropdown->currentIndex());
  if (iniswap != char_list.at(m_cid).name && !defswaplist.contains(iniswap)) {
    ui_iniswap_dropdown->removeItem(ui_iniswap_dropdown->currentIndex());
  }
  on_iniswap_dropdown_changed(0); // Reset back to original
  update_character(m_cid);
}

void Courtroom::set_sfx_dropdown()
{
  ui_sfx_dropdown->blockSignals(true);
  ui_sfx_dropdown->clear();
  if (m_cid == -1) {
    ui_sfx_dropdown->hide();
    ui_sfx_remove->hide();
    return;
  }
  // Initialzie character sound list first. Will be empty if not found.
  sound_list = ao_app->get_list_file(
      ao_app->get_character_path(current_char, "soundlist.ini"));

  // If AO2 sound list is empty, try to find the DRO one.
  if (sound_list.isEmpty()) {
    sound_list = ao_app->get_list_file(
        ao_app->get_character_path(current_char, "sounds.ini"));
  }

  // Append default sound list after the character sound list.
  sound_list += ao_app->get_list_file(VPath("soundlist.ini"));

  QStringList display_sounds;
  for (const QString &sound : qAsConst(sound_list)) {
    QStringList unpacked = sound.split("=");
    QString display = unpacked[0].trimmed();
    if (unpacked.size() > 1) {
      display = unpacked[1].trimmed();
    }

    display_sounds.append(display);
  }
  display_sounds.prepend("Nothing");
  display_sounds.prepend("Default");

  ui_sfx_dropdown->show();
  ui_sfx_dropdown->addItems(display_sounds);
  ui_sfx_dropdown->setCurrentIndex(0);
  ui_sfx_remove->hide();
  ui_sfx_dropdown->blockSignals(false);
}

void Courtroom::on_sfx_dropdown_changed(int p_index)
{
  custom_sfx = "";
  ui_ic_chat_message->setFocus();
  if (p_index == 0) {
      ui_sfx_remove->hide();
  }
}

void Courtroom::on_sfx_dropdown_custom(QString p_sfx)
{
  ui_sfx_remove->show();
  custom_sfx = p_sfx;
}

void Courtroom::on_sfx_context_menu_requested(const QPoint &pos)
{
  QMenu *menu = ui_sfx_dropdown->lineEdit()->createStandardContextMenu();

  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->addSeparator();
  // SFX is not "Nothing" or "Default"?
  if (get_char_sfx() != "0" && get_char_sfx() != "1" && get_char_sfx() != "-") {
    // Add an option to play the SFX
    menu->addAction(QString("Play " + get_char_sfx()), this, &Courtroom::on_sfx_play_clicked);;
  }
  if (file_exists(ao_app->get_real_path(
          ao_app->get_character_path(current_char, "soundlist.ini"))))
    menu->addAction(QString("Edit " + current_char + "/soundlist.ini"), this,
                    &Courtroom::on_sfx_edit_requested);
  else
    menu->addAction(QString("Edit base soundlist.ini"), this,
                    &Courtroom::on_sfx_edit_requested);
  if (!custom_sfx.isEmpty())
    menu->addAction(QString("Clear Edit Text"), this, &Courtroom::on_sfx_remove_clicked);
  menu->addSeparator();
  menu->addAction(QString("Open base sounds folder"), this,
                  [=] {
    QString p_path = get_base_path() + "sounds/general/";
    if (!dir_exists(p_path)) {
      return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
  }
  );
  menu->popup(ui_sfx_dropdown->mapToGlobal(pos));
}

void Courtroom::on_sfx_play_clicked()
{
  sfx_player->play(get_char_sfx(), get_current_char());
}

void Courtroom::on_sfx_edit_requested()
{
  QString p_path = ao_app->get_real_path(ao_app->get_character_path(current_char, "soundlist.ini"));
  if (!file_exists(p_path)) {
    p_path = ao_app->get_real_path(ao_app->get_character_path(current_char, "sounds.ini"));
  }

  if (!file_exists(p_path)) {
    p_path = ao_app->get_real_path(VPath("soundlist.ini"));
  }

  if (!file_exists(p_path)) {
    p_path = get_base_path() + "soundlist.ini";
  }
  QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
}

void Courtroom::on_sfx_remove_clicked()
{
  ui_sfx_remove->hide();
  ui_sfx_dropdown->setCurrentIndex(0);
  custom_sfx = "";
}

void Courtroom::set_effects_dropdown()
{
  ui_effects_dropdown->blockSignals(true);
  ui_effects_dropdown->clear();
  if (m_cid == -1) {
    ui_effects_dropdown->hide();
    return;
  }
  QStringList effectslist;
  effectslist.append(ao_app->get_effects(current_char));

  if (effectslist.empty()) {
    ui_effects_dropdown->hide();
    return;
  }

  effectslist.prepend(tr("None"));

  ui_effects_dropdown->show();
  ui_effects_dropdown->addItems(effectslist);

  // Make the icons
  for (int i = 0; i < ui_effects_dropdown->count(); ++i) {
    QString iconpath = ao_app->get_effect("icons/" + ui_effects_dropdown->itemText(i), current_char, "");
    ui_effects_dropdown->setItemIcon(i, QIcon(iconpath));
  }

  ui_effects_dropdown->setCurrentIndex(0);
  ui_effects_dropdown->blockSignals(false);
}

void Courtroom::on_effects_context_menu_requested(const QPoint &pos)
{
  QMenu *menu = new QMenu(this);

  menu->setAttribute(Qt::WA_DeleteOnClose);
  if (!ao_app->read_char_ini(current_char, "effects", "Options").isEmpty())
    menu->addAction(
        QString("Open misc/" +
                ao_app->read_char_ini(current_char, "effects", "Options") +
                " folder"),
        this, &Courtroom::on_character_effects_edit_requested);
  menu->addAction(QString("Open theme's effects folder"), this,
                  &Courtroom::on_effects_edit_requested);
  menu->popup(ui_effects_dropdown->mapToGlobal(pos));
}
void Courtroom::on_effects_edit_requested()
{
  QString p_path = ao_app->get_real_path(ao_app->get_theme_path("effects/"));
  if (!dir_exists(p_path)) {
    p_path = ao_app->get_real_path(ao_app->get_theme_path("effects/", "default"));
    if (!dir_exists(p_path)) {
      return;
    }
  }
  QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
}
void Courtroom::on_character_effects_edit_requested()
{
  QString p_effect = ao_app->read_char_ini(current_char, "effects", "Options");
  QString p_path = ao_app->get_real_path(VPath("misc/" + p_effect + "/"));
  if (!dir_exists(p_path))
    return;

  QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
}

void Courtroom::on_effects_dropdown_changed(int p_index)
{
  effect = ui_effects_dropdown->itemText(p_index);
  ui_ic_chat_message->setFocus();
}

bool Courtroom::effects_dropdown_find_and_set(QString effect)
{
  for (int i = 0; i < ui_effects_dropdown->count(); ++i) {
    QString entry = ui_effects_dropdown->itemText(i);
    if (entry == effect) {
      ui_effects_dropdown->setCurrentIndex(i);
      return true;
    }
  }
  return false;
}

QString Courtroom::get_char_sfx()
{
  if (!custom_sfx.isEmpty())
    return custom_sfx;
  int index = ui_sfx_dropdown->currentIndex();
  if (index == 0) { // Default
    return ao_app->get_sfx_name(current_char, current_emote);
  }
  if (index == 1) { // Nothing
    return "1";
  }
  QString sfx = sound_list[index-2].split("=")[0].trimmed();
  if (sfx == "")
    return "1";
  return sfx;
}

int Courtroom::get_char_sfx_delay()
{
  return ao_app->get_sfx_delay(current_char, current_emote);
}

void Courtroom::on_mute_list_clicked(QModelIndex p_index)
{
  QListWidgetItem *f_item = ui_mute_list->item(p_index.row());
  QString f_char = f_item->text();
  QString real_char;

  if (f_char.endsWith(" [x]"))
    real_char = f_char.left(f_char.size() - 4);
  else
    real_char = f_char;

  int f_cid = -1;

  for (int n_char = 0; n_char < char_list.size(); n_char++) {
    if (char_list.at(n_char).name == real_char)
      f_cid = n_char;
  }

  if (f_cid < 0 || f_cid >= char_list.size()) {
    qWarning() << "" << real_char << " not present in char_list";
    return;
  }

  if (mute_map.value(f_cid)) {
    mute_map.insert(f_cid, false);
    f_item->setText(real_char);
  }
  else {
    mute_map.insert(f_cid, true);
    f_item->setText(real_char + " [x]");
  }
}

void Courtroom::on_pair_list_clicked(QModelIndex p_index)
{
  QListWidgetItem *f_item = ui_pair_list->item(p_index.row());
  QString real_char = f_item->text();

  // Strip the selection markers: " [x]" or " [1]" = partner 1, " [2]" = partner 2
  if (real_char.endsWith(" [x]") || real_char.endsWith(" [1]") ||
      real_char.endsWith(" [2]"))
    real_char.chop(4);

  int f_cid = -1;
  for (int n_char = 0; n_char < char_list.size(); n_char++) {
    if (char_list.at(n_char).name == real_char)
      f_cid = n_char;
  }

  if (f_cid < 0 || f_cid >= char_list.size()) {
    qWarning() << "" << real_char << " not present in char_list";
    return;
  }

  // Which partner slot is being edited? Always slot 0 on stock servers.
  bool second = ao_app->ulta_supported && pair_slot == 1;
  int &slot_id = second ? other_charid2 : other_charid;
  int &other_slot_id = second ? other_charid : other_charid2;

  if (slot_id == f_cid) {
    // Clicking the current partner again clears the slot.
    slot_id = -1;
  }
  else {
    slot_id = f_cid;
    // The same character can't be both partners.
    if (other_slot_id == f_cid)
      other_slot_id = -1;
  }

  update_pair_list_marks();
}

void Courtroom::update_pair_list_marks()
{
  // Rebuild the sorted names (the list is sorted by name) and re-mark them.
  QStringList sorted_pair_list;

  for (const char_type &i_char : qAsConst(char_list))
    sorted_pair_list.append(i_char.name);

  sorted_pair_list.sort();

  QString name1, name2;
  if (other_charid > -1 && other_charid < char_list.size())
    name1 = char_list.at(other_charid).name;
  if (ao_app->ulta_supported && other_charid2 > -1 &&
      other_charid2 < char_list.size())
    name2 = char_list.at(other_charid2).name;

  for (int i = 0; i < ui_pair_list->count() && i < sorted_pair_list.size();
       i++) {
    QString text = sorted_pair_list.at(i);
    if (!name1.isEmpty() && text == name1)
      text += ao_app->ulta_supported ? " [1]" : " [x]"; // stock keeps "[x]"
    else if (!name2.isEmpty() && text == name2)
      text += " [2]";
    ui_pair_list->item(i)->setText(text);
  }
}

void Courtroom::on_music_list_double_clicked(QTreeWidgetItem *p_item,
                                             int column)
{
  if (is_muted)
    return;
  if (!Options::getInstance().stopMusicOnCategoryEnabled() && p_item->parent() == nullptr)
    return;
  column = 1; // Column 1 is always the metadata (which we want)
  QString p_song = p_item->text(column);
  QStringList packet_contents;
  packet_contents.append(p_song);
  packet_contents.append(QString::number(m_cid));
  if ((!ui_ic_chat_name->text().isEmpty() && ao_app->cccc_ic_supported) ||
      ao_app->effects_supported)
    packet_contents.append(ui_ic_chat_name->text());
  if (ao_app->effects_supported)
    packet_contents.append(QString::number(music_flags));
  ao_app->send_server_packet(new AOPacket("MC", packet_contents));
}

void Courtroom::on_music_list_context_menu_requested(const QPoint &pos)
{
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->addAction(QString(tr("Stop Current Song")), this, &Courtroom::music_stop);
  menu->addAction(QString(tr("Play Random Song")), this, &Courtroom::music_random);
  menu->addSeparator();
  menu->addAction(QString(tr("Expand All Categories")), this,
                  &Courtroom::music_list_expand_all);
  menu->addAction(QString(tr("Collapse All Categories")), this,
                  &Courtroom::music_list_collapse_all);
  menu->addSeparator();

  menu->addAction(new QAction(tr("Fade Out Previous"), this));
  menu->actions().back()->setCheckable(true);
  menu->actions().back()->setChecked(music_flags & FADE_OUT);
  connect(menu->actions().back(), &QAction::toggled, this,
          &Courtroom::music_fade_out);

  menu->addAction(new QAction(tr("Fade In"), this));
  menu->actions().back()->setCheckable(true);
  menu->actions().back()->setChecked(music_flags & FADE_IN);
  connect(menu->actions().back(), &QAction::toggled, this,
          &Courtroom::music_fade_in);

  menu->addAction(new QAction(tr("Synchronize"), this));
  menu->actions().back()->setCheckable(true);
  menu->actions().back()->setChecked(music_flags & SYNC_POS);
  connect(menu->actions().back(), &QAction::toggled, this,
          &Courtroom::music_synchronize);

  menu->addSeparator();
  menu->addAction(QString("Open base music folder"), this,
                  [=] {
    QString p_path = get_base_path() + "sounds/music/";
    if (!dir_exists(p_path)) {
      return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
  }
  );

  menu->popup(ui_music_list->mapToGlobal(pos));
}

void Courtroom::music_fade_out(bool toggle)
{
  if (toggle)
    music_flags |= FADE_OUT;
  else
    music_flags &= ~FADE_OUT;
}

void Courtroom::music_fade_in(bool toggle)
{
  if (toggle)
    music_flags |= FADE_IN;
  else
    music_flags &= ~FADE_IN;
}

void Courtroom::music_synchronize(bool toggle)
{
  if (toggle)
    music_flags |= SYNC_POS;
  else
    music_flags &= ~SYNC_POS;
}

void Courtroom::music_random()
{
  QList<QTreeWidgetItem *> clist;
  QTreeWidgetItemIterator it(ui_music_list,
                             QTreeWidgetItemIterator::NotHidden |
                                 QTreeWidgetItemIterator::NoChildren);
  while (*it) {
    if (!(*it)->parent() || (*it)->parent()->isExpanded()) { // add top level songs and songs in expanded categories
      clist += (*it);
    }
    ++it;
  }
  if (clist.length() == 0)
      return;
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
  on_music_list_double_clicked(clist.at(qrand() % clist.length()), 1);
#else
  on_music_list_double_clicked(clist.at(QRandomGenerator::global()->bounded(0, clist.length())), 1);
#endif
}

void Courtroom::music_list_expand_all() { ui_music_list->expandAll(); }
void Courtroom::music_list_collapse_all()
{
  ui_music_list->collapseAll();
  QTreeWidgetItem *current = ui_music_list->selectedItems()[0];
  if (current->parent() != nullptr)
    current = current->parent();
  ui_music_list->setCurrentItem(current);
}

void Courtroom::music_stop(bool no_effects)
{
  if (is_muted)
    return;
  // Default fake song is a song present in Vanilla content, the ~stop.mp3
  QString fake_song = "~stop.mp3";
  // If the fake song is not present in the music list
  if (!music_list.contains(fake_song)) {
      // Loop through our music list
      for (const QString &song : qAsConst(music_list)) {
          // Pick first song that does not contain a file extension
          if (!song.contains('.')) {
              // Use it as a fake song as the server we're working with must recognize song categories
              fake_song = song;
              break;
          }
      }
  }
  QStringList packet_contents; // its music list
  packet_contents.append(
      fake_song); // this is our fake song, playing it triggers special code
  packet_contents.append(QString::number(m_cid));
  if ((!ui_ic_chat_name->text().isEmpty() && ao_app->cccc_ic_supported) ||
      ao_app->effects_supported)
    packet_contents.append(ui_ic_chat_name->text());
  if (ao_app->effects_supported) {
    if (no_effects)
      packet_contents.append("0");
    else
      packet_contents.append(QString::number(music_flags));
  }
  ao_app->send_server_packet(new AOPacket("MC", packet_contents));
}

void Courtroom::on_area_list_double_clicked(QTreeWidgetItem *p_item, int column)
{
  column = 0; // The metadata
  Q_UNUSED(column); // so gcc shuts up
  QString p_area = p_item->text(0);

  QStringList packet_contents;
  packet_contents.append(p_area);
  packet_contents.append(QString::number(m_cid));
  ao_app->send_server_packet(new AOPacket("MC", packet_contents));
}

void Courtroom::on_hold_it_clicked()
{
  if (objection_state == 1) {
    ui_hold_it->set_image("holdit");
    objection_state = 0;
  }
  else {
    ui_objection->set_image("objection");
    ui_take_that->set_image("takethat");
    ui_custom_objection->set_image("custom");

    ui_hold_it->set_image("holdit_selected");
    objection_state = 1;
  }

  ui_ic_chat_message->setFocus();
}

void Courtroom::on_objection_clicked()
{
  if (objection_state == 2) {
    ui_objection->set_image("objection");
    objection_state = 0;
  }
  else {
    ui_hold_it->set_image("holdit");
    ui_take_that->set_image("takethat");
    ui_custom_objection->set_image("custom");

    ui_objection->set_image("objection_selected");
    objection_state = 2;
  }

  ui_ic_chat_message->setFocus();
}

void Courtroom::on_take_that_clicked()
{
  if (objection_state == 3) {
    ui_take_that->set_image("takethat");
    objection_state = 0;
  }
  else {
    ui_objection->set_image("objection");
    ui_hold_it->set_image("holdit");
    ui_custom_objection->set_image("custom");

    ui_take_that->set_image("takethat_selected");
    objection_state = 3;
  }

  ui_ic_chat_message->setFocus();
}

void Courtroom::on_custom_objection_clicked()
{
  if (objection_state == 4) {
    ui_custom_objection->set_image("custom");
    objection_state = 0;
  }
  else {
    ui_objection->set_image("objection");
    ui_take_that->set_image("takethat");
    ui_hold_it->set_image("holdit");

    ui_custom_objection->set_image("custom_selected");
    objection_state = 4;
  }

  ui_ic_chat_message->setFocus();
}

void Courtroom::show_custom_objection_menu(const QPoint &pos)
{
  QPoint globalPos = ui_custom_objection->mapToGlobal(pos);
  QAction *selecteditem = custom_obj_menu->exec(globalPos);
  if (selecteditem) {
    ui_objection->set_image("objection");
    ui_take_that->set_image("takethat");
    ui_hold_it->set_image("holdit");
    ui_custom_objection->set_image("custom_selected");
    if (selecteditem->text() == ao_app->read_char_ini(current_char, "custom_name", "Shouts") || selecteditem->text() == "Default")
      objection_custom = "";
    else {
      foreach (CustomObjection custom_objection, custom_objections_list) {
        if (custom_objection.name == selecteditem->text()) {
          objection_custom = custom_objection.filename;
          break;
        }
      }
    }
    objection_state = 4;
    custom_obj_menu->setDefaultAction(selecteditem);
  }
}

void Courtroom::on_realization_clicked()
{
  if (realization_state == 0) {
    realization_state = 1;
    if (effects_dropdown_find_and_set("realization"))
      on_effects_dropdown_changed(ui_effects_dropdown->currentIndex());

    ui_realization->set_image("realization_pressed");
  }
  else {
    realization_state = 0;
    ui_effects_dropdown->setCurrentIndex(0);
    on_effects_dropdown_changed(ui_effects_dropdown->currentIndex());
    ui_realization->set_image("realization");
  }

  ui_ic_chat_message->setFocus();
}

void Courtroom::on_screenshake_clicked()
{
  if (screenshake_state == 0) {
    screenshake_state = 1;
    ui_screenshake->set_image("screenshake_pressed");
  }
  else {
    screenshake_state = 0;
    ui_screenshake->set_image("screenshake");
  }

  ui_ic_chat_message->setFocus();
}

void Courtroom::sync_log_under_lists()
{
  // Hide the chat log while a list or pair control sits on top of it
  const QList<QWidget *> covers = {ui_pair_list, ui_mute_list,
                                   ui_pair_offset_spinbox,
                                   ui_pair_vert_offset_spinbox,
                                   ui_pair_order_dropdown, ui_pair_slot_dropdown};
  bool covered = false;
  for (QWidget *w : covers) {
    if (!w->isHidden() &&
        (w->geometry().intersects(ui_server_chatlog->geometry()) ||
         w->geometry().intersects(ui_debug_log->geometry())))
      covered = true;
  }
  ui_server_chatlog->setVisible(server_ooc && !covered);
  ui_debug_log->setVisible(!server_ooc && !covered);
}

void Courtroom::on_mute_clicked()
{
  if (ui_mute_list->isHidden()) {
    ui_mute_list->show();
    ui_pair_list->hide();
    ui_pair_offset_spinbox->hide();
    ui_pair_vert_offset_spinbox->hide();
    ui_pair_order_dropdown->hide();
    ui_pair_slot_dropdown->hide();
    ui_pair_button->set_image("pair_button");
    ui_mute->set_image("mute_pressed");
  }
  else {
    ui_mute_list->hide();
    ui_mute->set_image("mute");
  }
  sync_log_under_lists();
}

void Courtroom::on_pair_clicked()
{
  if (ui_pair_list->isHidden()) {
    ui_pair_list->show();
    ui_pair_offset_spinbox->show();
    if(ao_app->y_offset_supported)
        ui_pair_vert_offset_spinbox->show();
    ui_pair_order_dropdown->show();
    if (ao_app->ulta_supported)
      ui_pair_slot_dropdown->show();
    ui_mute_list->hide();
    ui_mute->set_image("mute");
    ui_pair_button->set_image("pair_button_pressed");
  }
  else {
    ui_pair_list->hide();
    ui_pair_offset_spinbox->hide();
    ui_pair_vert_offset_spinbox->hide();
    ui_pair_order_dropdown->hide();
    ui_pair_slot_dropdown->hide();
    ui_pair_button->set_image("pair_button");
  }
  sync_log_under_lists();
}

void Courtroom::on_pair_order_dropdown_changed(int p_index)
{
  pair_order = p_index;
}

void Courtroom::on_pair_slot_dropdown_changed(int p_index)
{
  pair_slot = p_index;
}

// ---- Narrator mode ----------------------------------------------------

void Courtroom::parse_narration(const QString &raw, bool &on, QString &folder,
                                int &scene, QString &name,
                                QString *tool_token) const
{
  // "folder;scene;name" with an optional scene token, anything else means not narrating
  on = false;
  if (tool_token)
    tool_token->clear();
  if (raw.isEmpty() || raw == "-")
    return;
  const QStringList parts = raw.split(";");
  if (parts.size() < 3)
    return;
  static const QRegularExpression folder_re("^[A-Za-z0-9_]{1,32}$");
  if (!folder_re.match(parts.at(0)).hasMatch())
    return;
  if (parts.at(1) != "0" && parts.at(1) != "1")
    return;
  QStringList name_parts = parts.mid(2);
  QString token;
  static const QRegularExpression token_re("^([bw][oi]h?|t([2-9]|1[0-9]|20))$");
  if (name_parts.size() >= 2 && token_re.match(name_parts.last()).hasMatch()) {
    token = name_parts.last();
    name_parts.removeLast();
  }
  const QString n = UltaNarratorDialog::sanitizeName(name_parts.join(";"));
  folder = parts.at(0);
  scene = parts.at(1).toInt();
  name = n;
  on = true;
  if (tool_token)
    *tool_token = token;
}

QString Courtroom::style_char() const
{
  return (m_narr_on && !m_narr_folder.isEmpty()) ? m_narr_folder
                                                  : m_chatmessage[CHAR_NAME];
}

QString Courtroom::ulta_narration_string() const
{
  if (!ulta_narrator_on || !ao_app->ulta_director)
    return "-";
  return ulta_narrator_folder + ";" + QString::number(ulta_narrator_scene) +
         ";" + UltaNarratorDialog::sanitizeName(ulta_narrator_name);
}

void Courtroom::load_ulta_narrator_settings()
{
  QSettings settings(get_base_path() + "ulta.ini", QSettings::IniFormat);
  settings.beginGroup("narrator");
  ulta_narrator_on = settings.value("on", false).toBool();
  ulta_narrator_scene = settings.value("scene", 0).toInt() == 1 ? 1 : 0;
  const QString folder = settings.value("folder", "NarratorULTA").toString();
  static const QRegularExpression folder_re("^[A-Za-z0-9_]{1,32}$");
  ulta_narrator_folder =
      folder_re.match(folder).hasMatch() ? folder : QString("NarratorULTA");
  ulta_narrator_name =
      UltaNarratorDialog::sanitizeName(settings.value("name", "Narrator").toString());
  ulta_narrator_panel_has_pos =
      settings.contains("panel_x") && settings.contains("panel_y");
  if (ulta_narrator_panel_has_pos)
    ulta_narrator_panel_pos = QPoint(settings.value("panel_x").toInt(),
                                     settings.value("panel_y").toInt());
  settings.endGroup();
}

void Courtroom::save_ulta_narrator_settings()
{
  QSettings settings(get_base_path() + "ulta.ini", QSettings::IniFormat);
  settings.beginGroup("narrator");
  settings.setValue("on", ulta_narrator_on);
  settings.setValue("scene", ulta_narrator_scene);
  settings.setValue("folder", ulta_narrator_folder);
  settings.setValue("name", ulta_narrator_name);
  if (ulta_narrator_panel_has_pos) {
    settings.setValue("panel_x", ulta_narrator_panel_pos.x());
    settings.setValue("panel_y", ulta_narrator_panel_pos.y());
  }
  settings.endGroup();
  settings.sync();
}

void Courtroom::update_narrator_button()
{
  ui_ulta_narrator->setVisible(m_ulta_nr_placed && ao_app->ulta_director);
  // The showname box gives its space to Nr only while Nr is shown
  if (m_nr_side > 0) {
    const bool show_nr = ao_app->ulta_director;
    if (show_nr && !m_nr_name_shrunk) {
      ui_ic_chat_name->resize(ui_ic_chat_name->width() - m_nr_side, ui_ic_chat_name->height());
      m_nr_name_shrunk = true;
    }
    else if (!show_nr && m_nr_name_shrunk) {
      ui_ic_chat_name->resize(ui_ic_chat_name->width() + m_nr_side, ui_ic_chat_name->height());
      m_nr_name_shrunk = false;
    }
  }
  ui_ulta_narrator->blockSignals(true);
  ui_ulta_narrator->setChecked(ulta_narrator_on);
  ui_ulta_narrator->blockSignals(false);
  ui_ulta_narrator->setToolTip(
      ulta_narrator_on
          ? tr("Narrator mode is ON: your messages are shown as narration "
               "(%1).\nClick to turn it off. Right-click for settings.")
                .arg(ulta_narrator_name)
          : tr("Narrator mode: show your messages as narration.\nClick to "
               "turn it on. Right-click for settings."));
  sync_narrator_panel();
}

void Courtroom::ulta_check_theme()
{
  if (!ao_app->ulta_supported)
    return;
  const QString theme = Options::getInstance().theme();
  if (theme == m_ulta_theme_noted)
    return;
  m_ulta_theme_noted = theme;

  // Look for the marker in the theme's own folder only, not through the
  // normal fallback chain, so a theme can't inherit the default theme's marker.
  QStringList bases = Options::getInstance().mountPaths();
  bases.push_front(get_base_path());
  bases.append(get_ulta_builtin_path());
  int version = -1;
  for (const QString &base : qAsConst(bases)) {
    const QString path = base + "themes/" + theme + "/ulta_theme.ini";
    if (QFile::exists(path)) {
      QSettings marker(path, QSettings::IniFormat);
      version = marker.value("ulta/version", 0).toInt();
      break;
    }
  }
  QString text;
  if (version < 0) {
    text = tr("This theme isn't ULTA approved. Some ULTA buttons may sit in "
              "default positions.");
  }
  else if (version < ULTA_THEME_VERSION) {
    text = tr("This ULTA theme is out of date. Some newer buttons may sit in "
              "default positions.");
  }
  else {
    return;
  }

  // The player can silence the popup per theme.
  QSettings ulta(get_base_path() + "ulta.ini", QSettings::IniFormat);
  QStringList ignored = ulta.value("theme/ignored_notices").toStringList();
  if (ignored.contains(theme))
    return;

  QMessageBox *box = new QMessageBox(QMessageBox::Information, tr("ULTA AO"),
                                     text, QMessageBox::Ok, this);
  box->setAttribute(Qt::WA_DeleteOnClose);
  // Own style: the popup is a child of the courtroom and would otherwise
  // inherit the theme's colors (light text on a light background).
  box->setStyleSheet(
      "QMessageBox { background-color: #171a21; }"
      "QLabel, QCheckBox { color: #e4e6eb; background-color: transparent; }"
      "QPushButton { color: #e4e6eb; background-color: #1e222b;"
      " border: 1px solid #2e3440; border-radius: 4px; padding: 4px 18px; }"
      "QPushButton:hover { border-color: #e0b441; }");
  QCheckBox *dont_show =
      new QCheckBox(tr("Don't show this again for this theme"), box);
  box->setCheckBox(dont_show);
  connect(box, &QDialog::finished, this, [theme, dont_show](int) {
    if (!dont_show->isChecked())
      return;
    QSettings s(get_base_path() + "ulta.ini", QSettings::IniFormat);
    QStringList list = s.value("theme/ignored_notices").toStringList();
    if (!list.contains(theme))
      list.append(theme);
    s.setValue("theme/ignored_notices", list);
    s.sync();
  });
  box->open(); // does not block the packet handler that called us
}

void Courtroom::set_ulta_area(const QString &area, const QString &cms)
{
  ao_app->ulta_area_name = area;
  ao_app->ulta_area_cms = (cms == "-") ? QString() : cms;
  update_ulta_status();
}

bool Courtroom::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == ui_evidence &&
      (event->type() == QEvent::Show || event->type() == QEvent::Hide))
    set_evidence_button_open(event->type() == QEvent::Show);
  // Ctrl+wheel would zoom the log text away from the theme's font size
  if (ui_ic_chatlog && watched == ui_ic_chatlog->viewport() && event->type() == QEvent::Wheel &&
      (static_cast<QWheelEvent *>(event)->modifiers() & Qt::ControlModifier))
    return true;
  return QMainWindow::eventFilter(watched, event);
}

void Courtroom::set_evidence_button_open(bool open)
{
  if (open) {
    // Only if the theme has an "open" image for it.
    const QString path = ao_app->get_image(
        "evidence_button_pressed", Options::getInstance().theme(),
        Options::getInstance().subTheme(), ao_app->default_theme, "", "", "",
        !Options::getInstance().animatedThemeEnabled());
    if (!path.isEmpty()) {
      ui_evidence_button->set_image("evidence_button_pressed");
      return;
    }
  }
  ui_evidence_button->set_image("evidence_button");
}

void Courtroom::update_ulta_status()
{
  if (!ui_ulta_status)
    return;
  // Only in the courtroom itself: not on the character select screen, and not
  // as a spectator without a character.
  const bool in_scene = m_cid != -1 && ui_char_select_background->isHidden();
  if (!ao_app->ulta_supported || !in_scene) {
    ui_ulta_status->hide();
    return;
  }
  const QString gold = "#e0b441";
  QStringList parts;
  if (!ao_app->ulta_area_name.isEmpty())
    parts << QString("<span style='color:%1'>%2</span>")
                 .arg(gold, ao_app->ulta_area_name.toHtmlEscaped());
  if (!ao_app->ulta_area_cms.isEmpty())
    parts << tr("CM: %1").arg(ao_app->ulta_area_cms.toHtmlEscaped());
  if (ulta_narrator_on && ao_app->ulta_director)
    parts << tr("Narrator on");
  if (m_flash_on) {
    QString flash = tr("Flashback: %1")
                        .arg(m_flash_look == "s" ? tr("sepia") : tr("black and white"));
    if (m_flash_hold)
      flash += tr(" (held)");
    parts << flash;
  }
  if (m_fade_on) {
    const bool white = ui_vp_fade && ui_vp_fade->fadeColor() == Qt::white;
    QString fade = white ? tr("Whiteout") : tr("Blackout");
    if (m_fade_hold)
      fade += tr(" (held)");
    parts << fade;
  }
  ui_ulta_status->setText(
      parts.join("&nbsp;&nbsp;<span style='color:#5d636f'>|</span>&nbsp;&nbsp;"));
  ui_ulta_status->setVisible(!parts.isEmpty());
}

void Courtroom::sync_ulta_director_ui()
{
  // The server says whether this player may use narrator mode and flashbacks here (area CM and mods only)
  update_narrator_button();
  sync_flash_panel();
  update_ulta_status();
}

void Courtroom::sync_narrator_panel()
{
  // Fill the panel from the saved settings, show it only while narrator mode is on
  ui_narrator_scene->blockSignals(true);
  ui_narrator_voice->blockSignals(true);
  ui_narrator_name->blockSignals(true);
  ui_narrator_scene->setCurrentIndex(ui_narrator_scene->findData(ulta_narrator_scene));
  int voice_index = ui_narrator_voice->findData(ulta_narrator_folder);
  ui_narrator_voice->setCurrentIndex(voice_index >= 0 ? voice_index : 0);
  ui_narrator_name->setText(ulta_narrator_name);
  ui_narrator_scene->blockSignals(false);
  ui_narrator_voice->blockSignals(false);
  ui_narrator_name->blockSignals(false);

  // Only while in the scene, not on character select or as a spectator
  const bool in_scene = m_cid != -1 && ui_char_select_background->isHidden();
  const bool visible = ulta_narrator_on && ao_app->ulta_supported &&
                       ao_app->ulta_director && in_scene;
  ui_narrator_panel->setVisible(visible);
  if (visible)
    ui_narrator_panel->raise();
  else
    disarm_scene_tool(); // no panel, no armed tool
  sync_flash_panel();
  update_ulta_status();
}

void Courtroom::reload_audio_device(bool announce)
{
  music_player->reload_device();
  update_audio_volume();
  if (announce) {
    append_server_chatmessage(
        "ULTA AO2",
        tr("Audio output reloaded. It now uses your current default output "
           "device."),
        "0");
  }
}

void Courtroom::on_audio_slider_context_menu(const QPoint &pos)
{
  QWidget *slider = qobject_cast<QWidget *>(sender());
  if (!slider)
    return;
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);
  QAction *reload = menu->addAction(tr("Reload audio output"));
  reload->setToolTip(tr("Rebuild the audio engine on the current output."));
  connect(reload, &QAction::triggered, this,
          [this]() { reload_audio_device(true); });

  // Pick a SPECIFIC output device. Under Wine the generic "Default" can stay
  // stuck on the device that was default when the client started; choosing the
  // device by name opens it directly, and it takes effect at once.
  QMenu *devices = menu->addMenu(tr("Audio output"));
  const QString current = ao_app->session_audio_device.isEmpty()
                              ? Options::getInstance().audioOutputDevice()
                              : ao_app->session_audio_device;
  BASS_DEVICEINFO info;
  for (int i = 1; BASS_GetDeviceInfo(i, &info); ++i) {
    const QString name = QString::fromUtf8(info.name);
    const QString stored = (i == 1) ? QStringLiteral("default") : name;
    QAction *device_action = devices->addAction(
        i == 1 ? tr("Default (follow the system)") : name);
    device_action->setCheckable(true);
    device_action->setChecked(current == stored ||
                              (i == 1 && current.isEmpty()));
    connect(device_action, &QAction::triggered, this, [this, stored, name, i]() {
      ao_app->session_audio_device = stored; // this run only
      reload_audio_device(false);
      const QString shown = i == 1 ? tr("Default (follow the system)") : name;
      if (ao_app->last_audio_error != 0) {
        append_server_chatmessage(
            "ULTA AO2",
            tr("Could not open \"%1\" (audio error %2), so the Default output "
               "is being used instead. If a device was disconnected and "
               "reconnected, choose \"Restart client\" in this menu.")
                .arg(shown)
                .arg(ao_app->last_audio_error),
            "0");
      }
      else {
        append_server_chatmessage(
            "ULTA AO2", tr("Audio output set to: %1").arg(shown), "0");
      }
    });
  }
  // A device that reconnected under a new identity can only be re-opened by a
  // fresh process (that is why a restart used to be needed). One click.
  QAction *restart = menu->addAction(tr("Restart client (fixes stuck audio)"));
  connect(restart, &QAction::triggered, this, []() {
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    QCoreApplication::quit();
  });
  menu->popup(slider->mapToGlobal(pos));
}

void Courtroom::arm_scene_tool(int emote_number)
{
  if (m_armed_tool == emote_number) {
    disarm_scene_tool(); // clicking the armed tool again cancels it
    return;
  }
  if (m_armed_tool == 0)
    m_tool_saved_placeholder = ui_ic_chat_message->placeholderText();
  m_armed_tool = emote_number;
  for (QPushButton *b : qAsConst(m_tool_buttons))
    b->setChecked(b->property("emote").toInt() == emote_number);
  ui_ic_chat_message->setPlaceholderText(
      tr("Scene tool selected: type a line (or leave empty) and press Enter"));
  ui_ic_chat_message->setFocus();
}

void Courtroom::raise_scene_fade()
{
  // Above the viewport, below the chatbox.
  ui_vp_fade->stackUnder(ui_vp_chatbox);
}

void Courtroom::sync_flash_panel()
{
  const bool in_scene = m_cid != -1 && ui_char_select_background->isHidden();
  const bool usable =
      ao_app->ulta_supported && ao_app->ulta_director && in_scene;
  ui_ulta_flash->setVisible(ao_app->ulta_supported && ao_app->ulta_director);
  ui_flash_panel->setState(m_flash_on, m_flash_on && m_flash_owner == m_cid,
                           m_flash_hold);
  const bool visible = usable && ui_ulta_flash->isChecked();
  ui_flash_panel->setVisible(visible);
  if (visible)
    ui_flash_panel->raise();
}

void Courtroom::apply_flash_state(const QString &look, int owner, bool hold)
{
  if (look != "s" && look != "b") {
    clear_flashback_local();
    return;
  }
  m_flash_on = true;
  m_flash_look = look;
  m_flash_owner = owner;
  m_flash_hold = hold;
  if (!m_flash_effect) {
    m_flash_effect = new UltaFlashbackEffect(ui_viewport);
    ui_viewport->setGraphicsEffect(m_flash_effect);
  }
  m_flash_effect->setLook(look == "s" ? UltaFlashbackEffect::Sepia
                                      : UltaFlashbackEffect::BlackWhite);
  sync_flash_panel();
  update_ulta_status();
}

void Courtroom::clear_flashback_local()
{
  m_flash_on = false;
  m_flash_owner = -1;
  m_flash_hold = false;
  if (m_flash_effect) {
    ui_viewport->setGraphicsEffect(nullptr); // deletes the effect
    m_flash_effect = nullptr;
  }
  sync_flash_panel();
  update_ulta_status();
}

void Courtroom::apply_scene_fx(const QString &token, int owner_cid)
{
  const bool white = token.startsWith('w');
  const bool out = token.size() > 1 && token.at(1) == 'o';
  const QColor wanted = white ? QColor(255, 255, 255) : QColor(0, 0, 0);
  // A fade of the same color continues, a different color clears the scene first (no instant black to
  // white jump)
  const bool same_color =
      ui_vp_fade->fadeLevel() > 0.0 && ui_vp_fade->fadeColor() == wanted;
  if (!same_color) {
    ui_vp_fade->clearNow();
    ui_vp_fade->setFadeColor(wanted);
  }
  raise_scene_fade();
  if (out) {
    m_fade_on = true;
    m_fade_owner = owner_cid;
    m_fade_hold = token.endsWith('h');
    ui_vp_fade->fadeTo(1.0, 900);
  }
  else {
    clear_scene_fx_state();
    // A fade in works alone: start fully covered and fade away
    if (ui_vp_fade->fadeLevel() <= 0.0)
      ui_vp_fade->setFadeLevel(1.0);
    ui_vp_fade->fadeTo(0.0, 900);
  }
  update_ulta_status();
}

void Courtroom::clear_scene_fx()
{
  clear_scene_fx_state();
  ui_vp_fade->clearNow();
  update_ulta_status();
}

void Courtroom::disarm_scene_tool()
{
  if (m_armed_tool == 0)
    return;
  m_armed_tool = 0;
  for (QPushButton *b : qAsConst(m_tool_buttons))
    b->setChecked(false);
  ui_ic_chat_message->setPlaceholderText(m_tool_saved_placeholder);
}

void Courtroom::play_tool_overlay(int tool)
{
  const QString nchar = m_narr_folder;
  if (nchar.isEmpty() || tool < 2 || tool > 20)
    return;
  const int e = tool - 1;
  const QString pre = ao_app->get_pre_emote(nchar, e);
  const int mod = ao_app->get_emote_mod(nchar, e);
  const QString sfx = ao_app->get_sfx_name(nchar, e);
  m_tool_overlay_char = nchar;
  m_tool_overlay_idle = "(a)" + ao_app->get_emote(nchar, e);

  if (!sfx.isEmpty() && sfx != "1")
    sfx_player->play(sfx);

  ui_vp_tool_overlay->stop();
  ui_vp_tool_overlay->show();
  if (mod != 0 && mod != 3 && !pre.isEmpty() && pre != "-") {
    m_tool_overlay_pre = true;
    const int duration = ao_app->get_preanim_duration(nchar, pre);
    ui_vp_tool_overlay->set_static_duration(duration);
    ui_vp_tool_overlay->set_play_once(true);
    ui_vp_tool_overlay->load_image(pre, nchar, duration, true);
  }
  else {
    m_tool_overlay_pre = false;
    ui_vp_tool_overlay->set_play_once(false);
    ui_vp_tool_overlay->load_image(m_tool_overlay_idle, nchar, 0, false);
  }
}

void Courtroom::tool_overlay_done()
{
  if (!m_tool_overlay_pre)
    return;
  m_tool_overlay_pre = false;
  ui_vp_tool_overlay->set_play_once(false);
  ui_vp_tool_overlay->load_image(m_tool_overlay_idle, m_tool_overlay_char, 0,
                                 false);
}

void Courtroom::clear_tool_overlay()
{
  m_tool_overlay_pre = false;
  ui_vp_tool_overlay->kill(); // also frees the loaded frames
  ui_vp_tool_overlay->hide();
}

QString Courtroom::scene_fx_token(int tool) const
{
  // Negative ids are fades: bo/bi = to/from black, wo/wi = to/from white, suffix h = hold
  static const QStringList fx_tokens = {"bo", "bi", "wo", "wi"};
  const int fx_index = -tool - 1;
  if (fx_index < 0 || fx_index >= fx_tokens.size())
    return QString();
  QString token = fx_tokens.at(fx_index);
  if ((token == "bo" || token == "wo") && ui_narrator_hold->isChecked())
    token += "h";
  return token;
}

void Courtroom::on_narrator_panel_changed()
{
  ulta_narrator_scene = ui_narrator_scene->currentData().toInt() == 1 ? 1 : 0;
  ulta_narrator_folder = ui_narrator_voice->currentData().toString();
  ulta_narrator_name =
      UltaNarratorDialog::sanitizeName(ui_narrator_name->text());
  save_ulta_narrator_settings();
  // Show the cleaned-up name and refresh the button's tooltip.
  ui_narrator_name->blockSignals(true);
  ui_narrator_name->setText(ulta_narrator_name);
  ui_narrator_name->blockSignals(false);
  ui_ulta_narrator->setToolTip(
      tr("Narrator mode is ON: your messages are shown as narration "
         "(%1).\nClick to turn it off. Right-click for settings.")
          .arg(ulta_narrator_name));
}

void Courtroom::on_ulta_narrator_toggled(bool p_on)
{
  ulta_narrator_on = p_on;
  save_ulta_narrator_settings();
  update_narrator_button();
  update_ulta_status();
  ui_ic_chat_message->setFocus();
}

void Courtroom::on_ulta_narrator_context_menu(const QPoint &pos)
{
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->addAction(tr("Narrator settings..."), this,
                  &Courtroom::open_ulta_narrator_settings);
  menu->popup(ui_ulta_narrator->mapToGlobal(pos));
}

void Courtroom::open_ulta_narrator_settings()
{
  UltaNarratorDialog dialog(ulta_narrator_scene, ulta_narrator_folder,
                            ulta_narrator_name, this);
  if (dialog.exec() == QDialog::Accepted) {
    ulta_narrator_scene = dialog.scene();
    ulta_narrator_folder = dialog.folder();
    ulta_narrator_name = dialog.name();
    save_ulta_narrator_settings();
    update_narrator_button();
  }
  ui_ic_chat_message->setFocus();
}

bool Courtroom::palette_marks_statement(const QString &raw)
{
  return raw.split(",", Qt::SkipEmptyParts).contains("s");
}

static double ulta_luminance(const QColor &c)
{
  auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
  return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

static double ulta_contrast(const QColor &a, const QColor &b)
{
  double la = ulta_luminance(a), lb = ulta_luminance(b);
  if (la < lb)
    std::swap(la, lb);
  return (la + 0.05) / (lb + 0.05);
}

QColor Courtroom::ulta_readable(const QColor &c, const QColor &bg) const
{
  const double target = 3.5;
  if (ulta_contrast(c, bg) >= target)
    return c;
  // Move toward white on a dark background, toward black on a light one
  const QColor goal = ulta_luminance(bg) < 0.4 ? QColor(255, 255, 255) : QColor(0, 0, 0);
  for (int step = 1; step <= 20; ++step) {
    const double t = step / 20.0;
    QColor mixed(qRound(c.red() + (goal.red() - c.red()) * t),
                 qRound(c.green() + (goal.green() - c.green()) * t),
                 qRound(c.blue() + (goal.blue() - c.blue()) * t));
    if (ulta_contrast(mixed, bg) >= target)
      return mixed;
  }
  return goal;
}

QColor Courtroom::log_background_color() const
{
  QWidget *view = ui_ic_chatlog->viewport();
  return view->palette().color(view->backgroundRole());
}

QColor Courtroom::message_background_color() const
{
  // Color of the chatbox art where the message text sits
  const QPixmap pm = ui_vp_chatbox->pixmap(Qt::ReturnByValue);
  if (pm.isNull())
    return QColor();
  const QImage img = pm.toImage();
  const int x = qBound(0, ui_vp_message->x() - ui_vp_chatbox->x() + 12, img.width() - 1);
  const int y = qBound(0, ui_vp_message->y() - ui_vp_chatbox->y() + 12, img.height() - 1);
  QColor c = img.pixelColor(x, y);
  c.setAlpha(255);
  return c;
}

QHash<int, QColor> Courtroom::parse_palette(const QString &raw, bool is_own, const QColor &bg) const
{
  QHash<int, QColor> result = parse_palette_raw(raw, is_own);
  if (ulta_fix_contrast && bg.isValid()) {
    for (auto it = result.begin(); it != result.end(); ++it)
      it.value() = ulta_readable(it.value(), bg);
  }
  return result;
}

QHash<int, QColor> Courtroom::parse_palette_raw(const QString &raw, bool is_own) const
{
  // "slot:rrggbb,slot:rrggbb" -> slot -> color. Entries that are not understood
  // are skipped; nothing here can crash on hostile input.
  QHash<int, QColor> result;
  if (!ao_app->ulta_supported)
    return result;
  // A testimony title always uses the stock colors
  if (raw.split(",", Qt::SkipEmptyParts).contains("t"))
    return result;

  // How other players' text looks is the player's choice, their own messages always use their own colors
  if (!is_own) {
    if (ulta_other_colors_mode == 1) // theme colors only
      return result;
    if (ulta_other_colors_mode == 2) { // our active hotslot, for everyone
      if (ulta_active_preset >= 0 && ulta_active_preset < ulta_presets.size())
        return ulta_presets.at(ulta_active_preset);
      return result;
    }
  }
  if (raw.isEmpty() || raw == "-")
    return result;

  const QStringList entries = raw.split(",", Qt::SkipEmptyParts);
  for (const QString &entry : entries) {
    const QStringList parts = entry.split(":");
    if (parts.size() != 2)
      continue;
    bool ok = false;
    int slot = parts.at(0).toInt(&ok);
    if (!ok || slot < 0 || slot >= max_colors)
      continue;
    if (parts.at(1).size() != 6)
      continue;
    QColor color("#" + parts.at(1));
    if (color.isValid())
      result.insert(slot, color);
  }
  return result;
}

QString Courtroom::ulta_palette_string() const
{
  if (ulta_active_preset < 0 || ulta_active_preset >= ulta_presets.size())
    return "-";
  const QHash<int, QColor> &palette = ulta_presets.at(ulta_active_preset);

  QStringList entries;
  QList<int> slot_list = palette.keys();
  std::sort(slot_list.begin(), slot_list.end());
  for (int slot : slot_list) {
    if (slot < 0 || slot >= max_colors)
      continue;
    entries.append(QString::number(slot) + ":" +
                   palette.value(slot).name(QColor::HexRgb).mid(1));
  }
  return entries.isEmpty() ? QString("-") : entries.join(",");
}

void Courtroom::load_ulta_color_settings()
{
  QSettings settings(get_base_path() + "ulta.ini", QSettings::IniFormat);
  settings.beginGroup("colors");
  // "other_mode" replaced the older on/off "show_others" setting; carry the old
  // choice over (off -> theme colors) if the new key isn't there yet.
  ulta_other_colors_mode =
      settings.contains("other_mode")
          ? settings.value("other_mode", 0).toInt()
          : (settings.value("show_others", true).toBool() ? 0 : 1);
  if (ulta_other_colors_mode < 0 || ulta_other_colors_mode > 2)
    ulta_other_colors_mode = 0;
  ulta_fix_contrast = settings.value("fix_contrast", false).toBool();
  ulta_active_preset = settings.value("active", 0).toInt();
  if (ulta_active_preset < 0 ||
      ulta_active_preset >= UltaPaletteDialog::PRESET_COUNT)
    ulta_active_preset = 0;

  ulta_presets.clear();
  for (int p = 0; p < UltaPaletteDialog::PRESET_COUNT; ++p) {
    QHash<int, QColor> palette;
    for (int slot = 0; slot < max_colors; ++slot) {
      QString value =
          settings.value(QString("p%1_%2").arg(p).arg(slot)).toString();
      if (value.isEmpty())
        continue;
      QColor color(value);
      if (color.isValid())
        palette.insert(slot, color);
    }
    ulta_presets.append(palette);
  }
  settings.endGroup();
}

void Courtroom::save_ulta_color_settings()
{
  QSettings settings(get_base_path() + "ulta.ini", QSettings::IniFormat);
  settings.beginGroup("colors");
  settings.setValue("other_mode", ulta_other_colors_mode);
  settings.setValue("fix_contrast", ulta_fix_contrast);
  settings.setValue("active", ulta_active_preset);
  for (int p = 0; p < ulta_presets.size(); ++p) {
    for (int slot = 0; slot < max_colors; ++slot) {
      QString key = QString("p%1_%2").arg(p).arg(slot);
      if (ulta_presets.at(p).contains(slot))
        settings.setValue(key, ulta_presets.at(p).value(slot).name(QColor::HexRgb));
      else
        settings.remove(key);
    }
  }
  settings.endGroup();
  settings.sync();
}

void Courtroom::on_ulta_colors_clicked()
{
  // One row per selectable text color (the ones in the text color dropdown).
  QVector<UltaSlotInfo> slot_list;
  for (int row = 0; row < color_row_to_number.size(); ++row) {
    int c = color_row_to_number.at(row);
    UltaSlotInfo info;
    info.slot = c;
    info.symbol = color_markdown_start_list.value(c);
    info.name = color_row_theme_names.value(row); // theme name, not the decorated label
    info.theme_color = color_rgb_list.value(c, QColor(255, 255, 255));
    slot_list.append(info);
  }

  UltaPaletteDialog dialog(slot_list, ulta_presets, ulta_active_preset,
                           ulta_other_colors_mode, ulta_fix_contrast, this);
  if (dialog.exec() != QDialog::Accepted) {
    ui_ic_chat_message->setFocus();
    return;
  }

  ulta_presets = dialog.presets();
  ulta_active_preset = dialog.activePreset();
  ulta_other_colors_mode = dialog.otherMode();
  ulta_fix_contrast = dialog.fixContrast();
  save_ulta_color_settings();
  update_text_color_icons();
  ui_ic_chat_message->setFocus();
}

void Courtroom::on_defense_minus_clicked()
{
  int f_state = defense_bar_state - 1;

  if (f_state >= 0)
    ao_app->send_server_packet(
        new AOPacket("HP", {"1", QString::number(f_state)}));
}

void Courtroom::on_defense_plus_clicked()
{
  int f_state = defense_bar_state + 1;

  if (f_state <= 10)
    ao_app->send_server_packet(
        new AOPacket("HP", {"1", QString::number(f_state)}));
}

void Courtroom::on_prosecution_minus_clicked()
{
  int f_state = prosecution_bar_state - 1;

  if (f_state >= 0)
    ao_app->send_server_packet(
        new AOPacket("HP", {"2", QString::number(f_state)}));
}

void Courtroom::on_prosecution_plus_clicked()
{
  int f_state = prosecution_bar_state + 1;

  if (f_state <= 10)
    ao_app->send_server_packet(
        new AOPacket("HP", {"2", QString::number(f_state)}));
}

void Courtroom::on_text_color_context_menu_requested(const QPoint &pos)
{
  QMenu *menu = new QMenu(this);

  menu->addAction(QString("Open currently used chat_config.ini"), this,
                  [=] {
    QString p_path = ao_app->get_asset("chat_config.ini", Options::getInstance().theme(), Options::getInstance().subTheme(), ao_app->default_theme, ao_app->get_chat(current_char));
    if (!file_exists(p_path)) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
  }
  );
  menu->popup(ui_text_color->mapToGlobal(pos));
}

void Courtroom::set_text_color_dropdown()
{
  // Clear the lists
  ui_text_color->clear();
  color_row_to_number.clear();
  color_row_theme_names.clear();

  // Clear the stored optimization information
  color_rgb_list.clear();
  default_color_rgb_list.clear();
  color_markdown_start_list.clear();
  color_markdown_end_list.clear();
  color_markdown_remove_list.clear();
  color_markdown_talking_list.clear();

  // Update markdown colors. TODO: make a loading function that only loads the
  // config file once instead of several times
  for (int c = 0; c < max_colors; ++c) {
    QColor color =
        ao_app->get_chat_color("c" + QString::number(c), ao_app->get_chat(current_char));
    color_rgb_list.append(color);
    color_markdown_start_list.append(ao_app->get_chat_markup(
        "c" + QString::number(c) + "_start", ao_app->get_chat(current_char)));
    color_markdown_end_list.append(ao_app->get_chat_markup(
        "c" + QString::number(c) + "_end", ao_app->get_chat(current_char)));
    color_markdown_remove_list.append(
        ao_app->get_chat_markup("c" + QString::number(c) + "_remove",
                                ao_app->get_chat(current_char)) == "1");
    color_markdown_talking_list.append(
        ao_app->get_chat_markup("c" + QString::number(c) + "_talking",
                                ao_app->get_chat(current_char)) != "0");

    QString color_name = ao_app->get_chat_markup(
        "c" + QString::number(c) + "_name", ao_app->get_chat(current_char));
    if (color_name.isEmpty()) // Not defined
    {
      if (c > 0)
        continue;
      color_name = tr("Default");
    }
    ui_text_color->addItem(color_name);
    QPixmap pixmap(16, 16);
    pixmap.fill(color);
    ui_text_color->setItemIcon(ui_text_color->count() - 1, pixmap);
    color_row_to_number.append(c);
    color_row_theme_names.append(color_name);
  }
  for (int c = 0; c < max_colors; ++c) {
    QColor color = ao_app->get_chat_color("c" + QString::number(c), "");
    default_color_rgb_list.append(color);
  }

  // Show the active hotslot colors in the swatches
  update_text_color_icons();
}

void Courtroom::update_text_color_icons()
{
  for (int row = 0; row < color_row_to_number.size() && row < ui_text_color->count(); ++row) {
    int c = color_row_to_number.at(row);
    QColor color = color_rgb_list.value(c, QColor(255, 255, 255));
    bool overridden = ao_app->ulta_supported && ulta_active_preset >= 0 &&
                      ulta_active_preset < ulta_presets.size() &&
                      ulta_presets.at(ulta_active_preset).contains(c);
    if (overridden)
      color = ulta_presets.at(ulta_active_preset).value(c);

    QPixmap pixmap(16, 16);
    pixmap.fill(color);
    ui_text_color->setItemIcon(row, pixmap);

    // ULTA servers: label "<symbol> <name>", named after the color if changed
    if (ao_app->ulta_supported) {
      QString theme_name = color_row_theme_names.value(row);
      ui_text_color->setItemText(
          row, UltaPaletteDialog::slotLabel(color_markdown_start_list.value(c),
                                            theme_name,
                                            overridden ? &color : nullptr));
      ui_text_color->setItemData(row, tr("Theme name: %1").arg(theme_name),
                                 Qt::ToolTipRole);
    }
  }
}

void Courtroom::gen_char_rgb_list(QString p_misc) {
  char_color_rgb_list.clear();
  for (int c = 0; c < max_colors; ++c) {
    QColor color = ao_app->get_chat_color("c" + QString::number(c), p_misc);
    char_color_rgb_list.append(color);
  }
}

void Courtroom::on_text_color_changed(int p_color)
{
  if (ui_ic_chat_message->selectionStart() != -1) // We have a selection!
  {
    int c = color_row_to_number.at(p_color);
    QString markdown_start = color_markdown_start_list.at(c);
    if (markdown_start.isEmpty()) {
      qWarning() << "Color list dropdown selected a non-existent markdown "
                  "start character";
      return;
    }
    QString markdown_end = color_markdown_end_list.at(c);
    if (markdown_end.isEmpty())
      markdown_end = markdown_start;
    int start = ui_ic_chat_message->selectionStart();
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    int end = ui_ic_chat_message->selectionEnd() + 1;
#else
    int end = ui_ic_chat_message->selectedText().length() + 1;
#endif

    ui_ic_chat_message->setCursorPosition(start);
    ui_ic_chat_message->insert(markdown_start);
    ui_ic_chat_message->setCursorPosition(end);
    ui_ic_chat_message->insert(markdown_end);
    //    ui_ic_chat_message->end(false);
    ui_text_color->setCurrentIndex(0);
  }
  else {
    if (p_color != -1 && p_color < color_row_to_number.size())
      text_color = color_row_to_number.at(p_color);
    else
      text_color = 0;
  }
  ui_ic_chat_message->setFocus();
}

void Courtroom::on_music_slider_moved(int p_value)
{
  music_player->set_volume(p_value, 0); // Set volume on music layer
  ui_ic_chat_message->setFocus();
}

void Courtroom::on_sfx_slider_moved(int p_value)
{
  sfx_player->set_volume(p_value);
  // Set the ambience and other misc. music layers
  for (int i = 1; i < music_player->m_channelmax; ++i) {
    music_player->set_volume(p_value, i);
  }
  objection_player->set_volume(p_value);
  ui_ic_chat_message->setFocus();
}

void Courtroom::on_blip_slider_moved(int p_value)
{
  blip_player->set_volume(p_value);
  ui_ic_chat_message->setFocus();
}

void Courtroom::on_log_limit_changed(int value) { log_maximum_blocks = value; }

void Courtroom::on_pair_offset_changed(int value) { char_offset = value; }

void Courtroom::on_pair_vert_offset_changed(int value)
{
  char_vert_offset = value;
}

void Courtroom::on_witness_testimony_clicked()
{
  if (is_muted)
    return;

  ao_app->send_server_packet(new AOPacket("RT", {"testimony1"}));

  ui_ic_chat_message->setFocus();
}

void Courtroom::on_cross_examination_clicked()
{
  if (is_muted)
    return;

  ao_app->send_server_packet(new AOPacket("RT", {"testimony2"}));

  ui_ic_chat_message->setFocus();
}

void Courtroom::on_not_guilty_clicked()
{
  if (is_muted)
    return;

  ao_app->send_server_packet(new AOPacket("RT", {"judgeruling", "0"}));

  ui_ic_chat_message->setFocus();
}

void Courtroom::on_guilty_clicked()
{
  if (is_muted)
    return;

  ao_app->send_server_packet(new AOPacket("RT", {"judgeruling", "1"}));

  ui_ic_chat_message->setFocus();
}

void Courtroom::on_change_character_clicked()
{
  sfx_player->set_muted(true);
  blip_player->set_muted(true);

  set_char_select();

  ui_char_select_background->show();
  // Raised widgets (buttons, clocks, effects) must not show through the select screen
  ui_char_select_background->raise();
  sync_narrator_panel(); // ULTA: hide the narrator panel behind the select screen
  ui_find_bar->closeBar();
}

void Courtroom::position_find_bar()
{
  // Open: the bar gets its own strip above the log. Closed: the log gets its rectangle back.
  const QRect r = m_ic_log_rect;
  if (!ui_find_bar->isVisible()) {
    ui_ic_chatlog->setGeometry(r);
    return;
  }
  const int bar_h = 30;
  const int top = 3;
  const int gap = 6;
  QScrollBar *sb = ui_ic_chatlog->verticalScrollBar();
  const bool at_bottom = sb->value() >= sb->maximum() - 2;
  const int sb_w = sb->isVisible() ? sb->width() : 0;
  const int bar_w = qMax(r.width() - 4 - sb_w - 8, 250);
  ui_find_bar->setGeometry(r.x() + 4, r.y() + top, bar_w, bar_h);
  const int used = top + bar_h + gap;
  ui_ic_chatlog->setGeometry(r.x(), r.y() + used, r.width(), qMax(r.height() - used, 40));
  if (at_bottom && log_goes_downwards)
    sb->setValue(sb->maximum());
  ui_find_bar->raise();
}

void Courtroom::on_reload_theme_clicked()
{
  m_debug_colors.clear();
  update_playing_song_highlight();
  set_courtroom_size();
  set_widgets();
  ulta_check_theme();
  update_character(m_cid, ui_iniswap_dropdown->itemText(ui_iniswap_dropdown->currentIndex()));
  enter_courtroom();
  gen_char_rgb_list(ao_app->get_chat(current_char));

  // to update status on the background
  set_background(current_background, true);
}

void Courtroom::on_back_to_lobby_clicked()
{
  ao_app->construct_lobby();
  ao_app->destruct_courtroom();
}

void Courtroom::on_char_select_left_clicked()
{
  --current_char_page;
  set_char_select_page();
}

void Courtroom::on_char_select_right_clicked()
{
  ++current_char_page;
  set_char_select_page();
}

void Courtroom::on_spectator_clicked() { char_clicked(-1); }

void Courtroom::on_call_mod_clicked()
{
  if (ao_app->modcall_reason_supported) {
    QMessageBox errorBox;
    QInputDialog input;

    input.setWindowFlags(Qt::WindowSystemMenuHint);
    input.setLabelText(tr("Reason:"));
    input.setWindowTitle(tr("Call Moderator"));
    auto code = input.exec();

    if (code != QDialog::Accepted)
      return;

    QString text = input.textValue();
    if (text.isEmpty()) {
      errorBox.critical(nullptr, tr("Error"), tr("You must provide a reason."));
      return;
    }
    else if (text.length() > 256) {
      errorBox.critical(nullptr, tr("Error"), tr("The message is too long."));
      return;
    }

    QStringList mod_reason;
    mod_reason.append(text);

    ao_app->send_server_packet(new AOPacket("ZZ", mod_reason));
  }
  else {
    ao_app->send_server_packet(new AOPacket("ZZ"));
  }

  ui_ic_chat_message->setFocus();
}

void Courtroom::on_settings_clicked() { ao_app->call_settings_menu(); }

void Courtroom::on_pre_clicked() { ui_ic_chat_message->setFocus(); }

void Courtroom::on_flip_clicked() { ui_ic_chat_message->setFocus(); }

void Courtroom::on_additive_clicked()
{
  if (ui_additive->isChecked()) {
    ui_ic_chat_message->home(false); // move cursor to the start of the message
    ui_ic_chat_message->insert(" "); // preface the message by whitespace
    ui_ic_chat_message->end(false);  // move cursor to the end of the message
                                     // without selecting anything
  }
  ui_ic_chat_message->setFocus();
}

void Courtroom::on_guard_clicked() { ui_ic_chat_message->setFocus(); }

void Courtroom::on_showname_enable_clicked()
{
  regenerate_ic_chatlog();
  ui_ic_chat_message->setFocus();
}

void Courtroom::regenerate_ic_chatlog()
{
  ui_ic_chatlog->clear();
  last_ic_message = "";
  foreach (chatlogpiece item, ic_chatlog_history) {
    QString message = item.get_message();
    QString name = ui_showname_enable->isChecked() ? item.get_showname()
                                                   : item.get_name();
    append_ic_text(message,
                   name,
                   item.get_action(), item.get_chat_color(),
                   item.get_selfname(), item.get_datetime().toLocalTime());
  }
}

void Courtroom::on_evidence_button_clicked()
{
  if (ui_evidence->isHidden()) {
    ui_evidence->show();
    ui_evidence_overlay->hide();
  }
  else {
    ui_evidence->hide();
  }
}

void Courtroom::on_evidence_context_menu_requested(const QPoint &pos)
{
  QMenu *menu = new QMenu(this);
  menu->addAction(QString("Open base evidence folder"), this,
                  [=] {
    QString p_path = get_base_path() + "evidence/";
    if (!dir_exists(p_path)) {
      return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(p_path));
  }
  );
  menu->popup(ui_evidence_button->mapToGlobal(pos));
}

void Courtroom::on_switch_area_music_clicked()
{
  if (ui_area_list->isHidden()) {
    ui_area_list->show();
    ui_music_list->hide();
    last_music_search = ui_music_search->text();
    ui_music_search->setText(last_area_search);
  }
  else {
    ui_area_list->hide();
    ui_music_list->show();
    last_area_search = ui_music_search->text();
    ui_music_search->setText(last_music_search);
  }
  on_music_search_edited(ui_music_search->text());

}

void Courtroom::ping_server()
{
  ping_timer.start();
  is_pinging = true;
  ao_app->send_server_packet(
      new AOPacket("CH", {QString::number(m_cid)}));
}

qint64 Courtroom::pong()
{
  if (!is_pinging)
    return -1;

  is_pinging = false;
  return ping_timer.elapsed();
}

void Courtroom::start_clock(int id)
{
  if (id >= 0 && id < max_clocks && ui_clock[id] != nullptr)
  {
    ui_clock[id]->start();
  }
}

void Courtroom::start_clock(int id, qint64 msecs)
{
  if (id >= 0 && id < max_clocks && ui_clock[id] != nullptr)
  {
    ui_clock[id]->start(msecs);
  }
}

void Courtroom::set_clock(int id, qint64 msecs)
{
  if (id >= 0 && id < max_clocks && ui_clock[id] != nullptr)
  {
    ui_clock[id]->set(msecs, true);
  }
}

// Used by demo playback to adjust for max_wait skips
void Courtroom::skip_clocks(qint64 msecs)
{
  // Loop through all the timers
  for (int i = 0; i < max_clocks; i++) {
    // Only skip time on active clocks
    if (ui_clock[i]->active())
      ui_clock[i]->skip(msecs);
  }
}

void Courtroom::pause_clock(int id)
{
  if (id >= 0 && id < max_clocks && ui_clock[id] != nullptr)
  {
    ui_clock[id]->pause();
  }
}

void Courtroom::stop_clock(int id)
{
  if (id >= 0 && id < max_clocks && ui_clock[id] != nullptr)
  {
    ui_clock[id]->stop();
  }
}

void Courtroom::set_clock_visibility(int id, bool visible)
{
  if (id >= 0 && id < max_clocks && ui_clock[id] != nullptr)
  {
    ui_clock[id]->setVisible(visible);
  }
}

void Courtroom::truncate_label_text(QWidget *p_widget, QString p_identifier)
{
  QString filename = "courtroom_design.ini";
  pos_size_type design_ini_result =
      ao_app->get_element_dimensions(p_identifier, filename);
  // Get the width of the element as defined by the current theme

  // Cast to make sure we're working with one of the two supported widget types
  QLabel *p_label = qobject_cast<QLabel *>(p_widget);
  QCheckBox *p_checkbox = qobject_cast<QCheckBox *>(p_widget);

  if (p_checkbox == nullptr &&
      p_label ==
          nullptr) { // i.e. the given p_widget isn't a QLabel or a QCheckBox
    qWarning() << "Tried to truncate an unsupported widget:" << p_identifier;
    return;
  }
  // translate the text for the widget we're working with so we truncate the right string
  QString label_text_tr =
      QCoreApplication::translate(p_widget->metaObject()->className(), "%1")
          .arg((p_label != nullptr ? p_label->text() : p_checkbox->text()));
  if (label_text_tr.endsWith("…") || label_text_tr.endsWith("…")) {
    qInfo() << "Truncation aborted for label text" << label_text_tr << ", label text was already truncated!";
    return;
  }

  int checkbox_width = AOApplication::style()->pixelMetric(QStyle::PM_IndicatorWidth) + AOApplication::style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing);

  int label_theme_width = (p_label != nullptr ? design_ini_result.width : (design_ini_result.width - checkbox_width));
  int label_px_width =
      p_widget->fontMetrics().boundingRect(label_text_tr).width(); // pixel width of our translated text
  if (!p_widget->toolTip().startsWith(label_text_tr)) // don't want to append this multiple times
    p_widget->setToolTip(label_text_tr + "\n" + p_widget->toolTip());

  // we can't do much with a 0-width widget, and there's no need to truncate if
  // the theme gives us enough space
  if (label_theme_width <= 0 || label_px_width < label_theme_width) {
    qDebug().nospace() << "Truncation aborted for label text " << label_text_tr
                       << ", either theme width <= 0 or label width < theme width.";
    return;
  }

  QString truncated_label = label_text_tr;
  int truncated_px_width = label_px_width;
  while (truncated_px_width > label_theme_width && truncated_label != "…") {
    truncated_label.chop(2);
    truncated_label.append("…");
    truncated_px_width =
        p_widget->fontMetrics().boundingRect(truncated_label).width();
  }
  if (truncated_label == "…") {
    // Safeguard against edge case where label text is shorter in px than '…',
    // causing an infinite loop. Additionally, having just an ellipse for a
    // label looks strange, so we don't set the new label.
    qWarning() << "Potential infinite loop prevented: Label text "
               << label_text_tr
               << "truncated to '…', so truncation was aborted.";
    return;
  }
  if (p_label != nullptr)
    p_label->setText(truncated_label);
  else if (p_checkbox != nullptr)
    p_checkbox->setText(truncated_label);
  qDebug().nospace() << "Truncated label text from " << label_text_tr << " ("
                     << label_px_width << "px) to " << truncated_label << " ("
                     << truncated_px_width << "px)";
}

Courtroom::~Courtroom()
{
  //save sound settings
  Options::getInstance().setMusicVolume(ui_music_slider->value());
  Options::getInstance().setSfxVolume(ui_sfx_slider->value());
  Options::getInstance().setBlipVolume(ui_blip_slider->value());

  delete music_player;
  delete sfx_player;
  delete objection_player;
  delete blip_player;
}

// ---- Character fade ---------------------------------------------------

void Courtroom::update_charfade_button()
{
  const bool armed = ulta_charfx_next != "-";
  const bool gone = m_charfx_gone.contains(m_cid) && !armed;
  ui_ulta_charfade->blockSignals(true);
  ui_ulta_charfade->setChecked(armed);
  ui_ulta_charfade->blockSignals(false);
  ui_ulta_charfade->setText(ulta_charfx_next == "o"   ? tr("Fo")
                            : ulta_charfx_next == "i" ? tr("Fi")
                            : gone                    ? tr("Gn")
                                                      : tr("Fd"));
  ui_ulta_charfade->setToolTip(
      gone ? tr("Your character is faded out (you are an unseen voice).\n"
                "Click and choose \"Fade in\" to bring it back.")
      : armed ? tr("Your next line will fade your character %1.\nClick to "
                 "change or cancel.")
                  .arg(ulta_charfx_next == "o" ? (ulta_charfx_keep ? tr("out and keep it gone")
                                                          : tr("out after the line"))
                                               : tr("in at the start"))
            : tr("Fade your character in or out with your next line "
                 "(only ULTA players see it)."));
}

void Courtroom::on_charfade_clicked()
{
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);
  QActionGroup *group = new QActionGroup(menu);
  auto add = [&](const QString &label, const QString &value) {
    QAction *a = menu->addAction(label);
    a->setCheckable(true);
    a->setChecked(ulta_charfx_next == value);
    group->addAction(a);
    connect(a, &QAction::triggered, this, [this, value]() {
      ulta_charfx_next = value;
      update_charfade_button();
      ui_ic_chat_message->setFocus();
    });
  };
  add(tr("No fade"), "-");
  add(tr("Fade in (appear with the line)"), "i");
  add(tr("Fade out (after the line)"), "o");
  menu->addSeparator();
  QAction *keep = menu->addAction(tr("Stay faded out until I fade back in"));
  keep->setCheckable(true);
  keep->setChecked(ulta_charfx_keep);
  keep->setToolTip(tr("Your character stays gone through your next lines "
                      "(you are an unseen voice) until a \"Fade in\"."));
  connect(keep, &QAction::toggled, this, [this](bool on) {
    ulta_charfx_keep = on;
    update_charfade_button();
  });
  connect(menu, &QMenu::aboutToHide, this, [this]() { update_charfade_button(); });
  menu->popup(ui_ulta_charfade->mapToGlobal(QPoint(0, ui_ulta_charfade->height())));
  update_charfade_button();
}

void Courtroom::reset_char_fx()
{
  m_charfx_out_pending = false;
  if (m_charfx_anim) {
    m_charfx_anim->stop();
    m_charfx_anim->clear();
  }
  ui_vp_player_char->setLayerOpacity(1.0);
}

void Courtroom::prepare_charfx_anim()
{
  if (!m_charfx_anim)
    m_charfx_anim = new QSequentialAnimationGroup(this);
  m_charfx_anim->stop();
  m_charfx_anim->clear();
}

void Courtroom::start_char_fx_in()
{
  prepare_charfx_anim();
  ui_vp_player_char->setLayerOpacity(0.0);
  QPropertyAnimation *a =
      new QPropertyAnimation(ui_vp_player_char, "layerOpacity");
  a->setDuration(800);
  a->setStartValue(0.0);
  a->setEndValue(1.0);
  a->setEasingCurve(QEasingCurve::InOutQuad);
  m_charfx_anim->addAnimation(a);
  m_charfx_anim->start();
}

void Courtroom::start_char_fx_out()
{
  m_charfx_out_pending = false;
  prepare_charfx_anim();
  m_charfx_anim->addPause(500); // let the last words land first
  QPropertyAnimation *a =
      new QPropertyAnimation(ui_vp_player_char, "layerOpacity");
  a->setDuration(800);
  a->setStartValue(1.0);
  a->setEndValue(0.0);
  a->setEasingCurve(QEasingCurve::InOutQuad);
  m_charfx_anim->addAnimation(a);
  m_charfx_anim->start();
}

void Courtroom::apply_scene_state(const QString &fade, int owner, bool hold,
                                  const QString &gone_csv)
{
  // Characters kept faded out: replace the local set with the server's.
  m_charfx_gone.clear();
  if (gone_csv != "-") {
    const QStringList ids = gone_csv.split(",", Qt::SkipEmptyParts);
    for (const QString &id : ids) {
      bool ok = false;
      const int cid = id.toInt(&ok);
      if (ok)
        m_charfx_gone.insert(cid);
    }
  }
  update_charfade_button();

  if (fade == "b" || fade == "w") {
    // Joined a dark/white scene: show it at once (no animation to watch).
    ui_vp_fade->clearNow();
    ui_vp_fade->setFadeColor(fade == "w" ? QColor(255, 255, 255)
                                         : QColor(0, 0, 0));
    ui_vp_fade->setFadeLevel(1.0);
    raise_scene_fade();
    m_fade_on = true;
    m_fade_owner = owner;
    m_fade_hold = hold;
  }
  else if (m_fade_on || ui_vp_fade->fadeLevel() > 0.0) {
    // The server says the scene is clear again (the owner left, etc.).
    clear_scene_fx_state();
    ui_vp_fade->fadeTo(0.0, 900);
  }
  update_ulta_status();
}

void Courtroom::on_viewport_context_menu(const QPoint &pos)
{
  if (!ao_app->ulta_supported)
    return;
  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);
  QAction *clear = menu->addAction(
      tr("Clear scene effects (this client only)"), this, [this]() {
        clear_scene_fx();
        clear_tool_overlay();
        clear_flashback_local();
        m_charfx_gone.clear();
        m_charfx_away.clear();
        update_charfade_button();
      });
  clear->setEnabled(m_fade_on || ui_vp_fade->fadeLevel() > 0.0 ||
                    !m_charfx_gone.isEmpty() || !m_charfx_away.isEmpty() ||
                    ui_vp_tool_overlay->isVisible() || m_flash_on);
  menu->popup(ui_viewport->mapToGlobal(pos));
}
