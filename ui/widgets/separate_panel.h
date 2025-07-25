// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/flat_map.h"
#include "base/weak_ptr.h"
#include "ui/rp_widget.h"
#include "ui/effects/animations.h"
#include "ui/layers/layer_widget.h"
#include "ui/text/text_entity.h"

#include <rpl/variable.h>

class Painter;

namespace style {
struct IconButton;
struct PopupMenu;
} // namespace style

namespace Ui::Menu {
struct MenuCallback;
} // namespace Ui::Menu

namespace Ui::Toast {
struct Config;
class Instance;
} // namespace Ui::Toast

namespace Ui {

class Show;
class BoxContent;
class IconButton;
class PopupMenu;
class LayerStackWidget;
class LayerWidget;
class FlatLabel;
class InputField;
template <typename Widget>
class FadeWrapScaled;
template <typename Widget>
class FadeWrap;

struct SeparatePanelArgs {
	QWidget *parent = nullptr;
	bool onAllSpaces = false;
	Fn<bool(int zorder)> animationsPaused;
	const style::PopupMenu *menuSt = nullptr;
};

class SeparatePanel final : public RpWidget {
public:
	explicit SeparatePanel(SeparatePanelArgs &&args = {});
	~SeparatePanel();

	void setTitle(rpl::producer<QString> title);
	void setTitleHeight(int height);
	void setTitleBadge(object_ptr<RpWidget> badge);
	void setInnerSize(QSize size, bool allowResize = false);
	[[nodiscard]] QRect innerGeometry() const;

	void toggleFullScreen(bool fullscreen);
	void allowChildFullScreenControls(bool allow);
	[[nodiscard]] rpl::producer<bool> fullScreenValue() const;
	[[nodiscard]] QMargins computePadding() const;

	void setHideOnDeactivate(bool hideOnDeactivate);
	void showAndActivate();
	int hideGetDuration();

	[[nodiscard]] RpWidget *inner() const;
	void showInner(base::unique_qptr<RpWidget> inner);
	void showBox(
		object_ptr<BoxContent> box,
		LayerOptions options,
		anim::type animated);
	void showLayer(
		std::unique_ptr<LayerWidget> layer,
		LayerOptions options,
		anim::type animated);
	void hideLayer(anim::type animated);

	[[nodiscard]] rpl::producer<> backRequests() const;
	[[nodiscard]] rpl::producer<> closeRequests() const;
	[[nodiscard]] rpl::producer<> closeEvents() const;
	void setBackAllowed(bool allowed);

	void updateBackToggled();

	void setMenuAllowed(
		Fn<void(const Menu::MenuCallback&)> fill,
		Fn<void(not_null<RpWidget*>, bool fullscreen)> created = nullptr);
	void setSearchAllowed(
		rpl::producer<QString> placeholder,
		Fn<void(std::optional<QString>)> queryChanged);
	bool closeSearch();

	void overrideTitleColor(std::optional<QColor> color);
	void overrideBodyColor(std::optional<QColor> color);
	void overrideBottomBarColor(std::optional<QColor> color);
	void setBottomBarHeight(int height);
	[[nodiscard]] style::palette *titleOverridePalette() const;

	base::weak_ptr<Toast::Instance> showToast(Toast::Config &&config);
	base::weak_ptr<Toast::Instance> showToast(
		TextWithEntities &&text,
		crl::time duration = 0);
	base::weak_ptr<Toast::Instance> showToast(
		const QString &text,
		crl::time duration = 0);

	[[nodiscard]] std::shared_ptr<Show> uiShow();

protected:
	void paintEvent(QPaintEvent *e) override;
	void closeEvent(QCloseEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void focusInEvent(QFocusEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	void leaveToChildEvent(QEvent *e, QWidget *child) override;
	void keyPressEvent(QKeyEvent *e) override;
	bool eventHook(QEvent *e) override;

private:
	class ResizeEdge;
	class FullScreenButton;

	struct BgColors {
		QColor title;
		QColor bg;
		QColor footer;
	};

	void initControls();
	void initLayout(const SeparatePanelArgs &args);
	void initGeometry(QSize size);
	void updateGeometry(QSize size);
	void showControls();
	void updateControlsGeometry();
	void updateControlsVisibility(bool fullscreen);
	void validateBorderImage();
	[[nodiscard]] QPixmap createBorderImage(QColor color) const;
	void opacityCallback();
	void ensureLayerCreated();
	void destroyLayer();

	void updateTitleGeometry(int newWidth) const;
	void paintShadowBorder(QPainter &p) const;
	void paintOpaqueBorder(QPainter &p) const;
	void paintBodyBg(QPainter &p, int radius = 0) const;

	void toggleOpacityAnimation(bool visible);
	void finishAnimating();
	void finishClose();

	void showMenu(Fn<void(const Menu::MenuCallback&)> fill);
	[[nodiscard]] bool createMenu(not_null<IconButton*> button);

	void createFullScreenButtons();
	void initFullScreenButton(not_null<QWidget*> button);
	void updateTitleButtonColors(not_null<IconButton*> button);
	void updateTitleColors();

	[[nodiscard]] BgColors computeBgColors() const;

	void toggleSearch(bool shown);
	[[nodiscard]] rpl::producer<> allBackRequests() const;
	[[nodiscard]] rpl::producer<> allCloseRequests() const;

	const style::PopupMenu &_menuSt;
	object_ptr<IconButton> _close;
	object_ptr<IconButton> _menuToggle = { nullptr };
	Fn<void(not_null<RpWidget*>, bool fullscreen)> _menuToggleCreated;
	object_ptr<FadeWrapScaled<IconButton>> _searchToggle = { nullptr };
	rpl::variable<QString> _searchPlaceholder;
	Fn<void(std::optional<QString>)> _searchQueryChanged;
	object_ptr<FadeWrap<RpWidget>> _searchWrap = { nullptr };
	InputField *_searchField = nullptr;
	object_ptr<FlatLabel> _title = { nullptr };
	object_ptr<RpWidget> _titleBadge = { nullptr };
	object_ptr<FadeWrapScaled<IconButton>> _back;
	object_ptr<RpWidget> _body;
	base::unique_qptr<RpWidget> _inner;
	base::unique_qptr<LayerStackWidget> _layer = { nullptr };
	base::unique_qptr<PopupMenu> _menu;
	std::vector<std::unique_ptr<ResizeEdge>> _resizeEdges;

	std::unique_ptr<FullScreenButton> _fsClose;
	std::unique_ptr<FullScreenButton> _fsMenuToggle;
	std::unique_ptr<FadeWrapScaled<FullScreenButton>> _fsBack;
	bool _fsAllowChildControls = false;

	rpl::event_stream<> _synteticBackRequests;
	rpl::event_stream<> _userCloseRequests;
	rpl::event_stream<> _closeEvents;

	int _titleHeight = 0;
	bool _allowResize = false;
	bool _hideOnDeactivate = false;
	bool _useTransparency = true;
	bool _backAllowed = false;
	style::margins _padding;

	bool _dragging = false;
	QPoint _dragStartMousePosition;
	QPoint _dragStartMyPosition;

	Animations::Simple _titleLeft;
	bool _visible = false;
	rpl::variable<bool> _fullscreen = false;

	Animations::Simple _opacityAnimation;
	QPixmap _animationCache;
	QPixmap _borderParts;

	std::optional<QColor> _titleOverrideColor;
	QPixmap _titleOverrideBorderParts;
	std::unique_ptr<style::palette> _titleOverridePalette;
	base::flat_map<
		not_null<IconButton*>,
		std::unique_ptr<style::IconButton>> _titleOverrideStyles;

	std::optional<QColor> _bodyOverrideColor;
	QPixmap _bodyOverrideBorderParts;

	std::optional<QColor> _bottomBarOverrideColor;
	QPixmap _bottomBarOverrideBorderParts;
	int _bottomBarHeight = 0;

	Fn<bool(int zorder)> _animationsPaused;

};

} // namespace Ui
