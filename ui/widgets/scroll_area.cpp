// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/widgets/scroll_area.h"

#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "base/qt/qt_common_adapters.h"
#include "base/debug_log.h"

#include <QtWidgets/QScrollBar>
#include <QtWidgets/QApplication>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

namespace Ui {
namespace {

[[nodiscard]] int ComputeScrollTo(
		int toFrom,
		int toTill,
		int toMin,
		int toMax,
		int current,
		int size) {
	if (toFrom < toMin) {
		toFrom = toMin;
	} else if (toFrom > toMax) {
		toFrom = toMax;
	}
	const auto exact = (toTill < 0);

	const auto curBottom = current + size;
	auto scToFrom = toFrom;
	if (!exact && toFrom >= current) {
		if (toTill < toFrom) {
			toTill = toFrom;
		}
		if (toTill <= curBottom) {
			return current;
		}

		scToFrom = toTill - size;
		if (scToFrom > toFrom) {
			scToFrom = toFrom;
		}
		if (scToFrom == current) {
			return current;
		}
	} else {
		scToFrom = toFrom;
	}
	return scToFrom;
}

} // namespace

// flick scroll taken from http://qt-project.org/doc/qt-4.8/demos-embedded-anomaly-src-flickcharm-cpp.html

ScrollShadow::ScrollShadow(ScrollArea *parent, const style::ScrollArea *st)
: QWidget(parent)
, _st(st) {
	Expects(_st != nullptr);
	Expects(_st->shColor.get() != nullptr);

	setVisible(false);
}

void ScrollShadow::paintEvent(QPaintEvent *e) {
	QPainter p(this);
	p.fillRect(rect(), _st->shColor);
}

void ScrollShadow::changeVisibility(bool shown) {
	setVisible(shown);
}

ScrollBar::ScrollBar(ScrollArea *parent, bool vert, const style::ScrollArea *st) : TWidget(parent)
, _st(st)
, _vertical(vert)
, _hiding(_st->hiding != 0)
, _connected(vert ? parent->verticalScrollBar() : parent->horizontalScrollBar())
, _scrollMax(_connected->maximum())
, _hideTimer([=] { hideTimer(); }) {
	recountSize();

	connect(_connected, &QAbstractSlider::valueChanged, [=] {
		area()->scrolled();
		updateBar();
	});
	connect(_connected, &QAbstractSlider::rangeChanged, [=] {
		area()->innerResized();
		updateBar();
	});

	updateBar();
}

void ScrollBar::recountSize() {
	setGeometry(_vertical
		? QRect(
			style::RightToLeft() ? 0 : (area()->width() - _st->width),
			_st->deltat,
			_st->width,
			area()->height() - _st->deltat - _st->deltab)
		: QRect(
			_st->deltat,
			area()->height() - _st->width,
			area()->width() - _st->deltat - _st->deltab,
			_st->width));
}

void ScrollBar::updateBar(bool force) {
	QRect newBar;
	if (_connected->maximum() != _scrollMax) {
		const auto oldMax = _scrollMax;
		const auto newMax = _connected->maximum();
		_scrollMax = newMax;
		area()->rangeChanged(oldMax, newMax, _vertical);
	}
	if (_vertical) {
		const auto sh = area()->scrollHeight();
		const auto rh = height();
		auto h = sh ? int32((rh * int64(area()->height())) / sh) : 0;
		if (_st->barHidden
			|| h >= rh
			|| !area()->scrollTopMax()
			|| rh < _st->minHeight) {
			if (!isHidden()) {
				hide();
			}
			const auto newTopSh = (_st->topsh < 0);
			const auto newBottomSh = (_st->bottomsh < 0);
			if (newTopSh != _topSh || force) {
				_shadowVisibilityChanged.fire({
					.type = ScrollShadow::Type::Top,
					.visible = (_topSh = newTopSh),
				});
			}
			if (newBottomSh != _bottomSh || force) {
				_shadowVisibilityChanged.fire({
					.type = ScrollShadow::Type::Bottom,
					.visible = (_bottomSh = newBottomSh),
				});
			}
			return;
		}

		if (h <= _st->minHeight) {
			h = _st->minHeight;
		}
		const auto stm = area()->scrollTopMax();
		const auto y = stm
			? std::min(
				int32(((rh - h) * int64(area()->scrollTop())) / stm),
				rh - h)
			: 0;

		newBar = QRect(_st->deltax, y, width() - 2 * _st->deltax, h);
	} else {
		const auto sw = area()->scrollWidth();
		const auto rw = width();
		auto w = sw ? int32((rw * int64(area()->width())) / sw) : 0;
		if (_st->barHidden
			|| w >= rw
			|| !area()->scrollLeftMax()
			|| rw < _st->minHeight) {
			if (!isHidden()) {
				hide();
			}
			return;
		}

		if (w <= _st->minHeight) {
			w = _st->minHeight;
		}
		const auto slm = area()->scrollLeftMax();
		const auto x = slm
			? std::min(
				int32(((rw - w) * int64(area()->scrollLeft())) / slm),
				rw - w)
			: 0;

		newBar = QRect(x, _st->deltax, w, height() - 2 * _st->deltax);
	}
	if (newBar != _bar) {
		_bar = newBar;
		update();
	}
	if (_vertical) {
		const auto newTopSh = (_st->topsh < 0)
			|| (area()->scrollTop() > _st->topsh);
		const auto newBottomSh = (_st->bottomsh < 0)
			|| (area()->scrollTop()
				< area()->scrollTopMax() - _st->bottomsh);
		if (newTopSh != _topSh || force) {
			_shadowVisibilityChanged.fire({
				.type = ScrollShadow::Type::Top,
				.visible = (_topSh = newTopSh),
			});
		}
		if (newBottomSh != _bottomSh || force) {
			_shadowVisibilityChanged.fire({
				.type = ScrollShadow::Type::Bottom,
				.visible = (_bottomSh = newBottomSh),
			});
		}
	}
	if (isHidden()) show();
}

void ScrollBar::hideTimer() {
	if (!_hiding) {
		_hiding = true;
		_a_opacity.start([this] { update(); }, 1., 0., _st->duration);
	}
}

ScrollArea *ScrollBar::area() {
	return static_cast<ScrollArea*>(parentWidget());
}

void ScrollBar::setOver(bool over) {
	if (_over != over) {
		auto wasOver = (_over || _moving);
		_over = over;
		auto nowOver = (_over || _moving);
		if (wasOver != nowOver) {
			_a_over.start([this] { update(); }, nowOver ? 0. : 1., nowOver ? 1. : 0., _st->duration);
		}
		if (nowOver && _hiding) {
			_hiding = false;
			_a_opacity.start([this] { update(); }, 0., 1., _st->duration);
		}
	}
}

void ScrollBar::setOverBar(bool overbar) {
	if (_overbar != overbar) {
		auto wasBarOver = (_overbar || _moving);
		_overbar = overbar;
		auto nowBarOver = (_overbar || _moving);
		if (wasBarOver != nowBarOver) {
			_a_barOver.start([this] { update(); }, nowBarOver ? 0. : 1., nowBarOver ? 1. : 0., _st->duration);
		}
	}
}

void ScrollBar::setMoving(bool moving) {
	if (_moving != moving) {
		auto wasOver = (_over || _moving);
		auto wasBarOver = (_overbar || _moving);
		_moving = moving;
		auto nowBarOver = (_overbar || _moving);
		if (wasBarOver != nowBarOver) {
			_a_barOver.start([this] { update(); }, nowBarOver ? 0. : 1., nowBarOver ? 1. : 0., _st->duration);
		}
		auto nowOver = (_over || _moving);
		if (wasOver != nowOver) {
			_a_over.start([this] { update(); }, nowOver ? 0. : 1., nowOver ? 1. : 0., _st->duration);
		}
		if (!nowOver && _st->hiding && !_hiding) {
			_hideTimer.callOnce(_hideIn);
		}
	}
}

void ScrollBar::paintEvent(QPaintEvent *e) {
	if (!_bar.width() && !_bar.height()) {
		hide();
		return;
	}
	auto opacity = _a_opacity.value(_hiding ? 0. : 1.);
	if (opacity == 0.) return;

	QPainter p(this);
	auto deltal = _vertical ? _st->deltax : 0, deltar = _vertical ? _st->deltax : 0;
	auto deltat = _vertical ? 0 : _st->deltax, deltab = _vertical ? 0 : _st->deltax;
	p.setPen(Qt::NoPen);
	auto bg = anim::color(_st->bg, _st->bgOver, _a_over.value((_over || _moving) ? 1. : 0.));
	bg.setAlpha(anim::interpolate(0, bg.alpha(), opacity));
	auto bar = anim::color(_st->barBg, _st->barBgOver, _a_barOver.value((_overbar || _moving) ? 1. : 0.));
	bar.setAlpha(anim::interpolate(0, bar.alpha(), opacity));
	const auto outer = QRect(deltal, deltat, width() - deltal - deltar, height() - deltat - deltab);
	const auto radius = (_st->round < 0)
		? (std::min(outer.width(), outer.height()) / 2.)
		: _st->round;
	if (radius) {
		PainterHighQualityEnabler hq(p);
		p.setBrush(bg);
		p.drawRoundedRect(outer, radius, radius);
		p.setBrush(bar);
		p.drawRoundedRect(_bar, radius, radius);
	} else {
		p.fillRect(outer, bg);
		p.fillRect(_bar, bar);
	}
}

void ScrollBar::hideTimeout(crl::time dt) {
	if (_hiding && dt > 0) {
		_hiding = false;
		_a_opacity.start([this] { update(); }, 0., 1., _st->duration);
	}
	_hideIn = dt;
	if (!_moving) {
		_hideTimer.callOnce(_hideIn);
	}
}

void ScrollBar::enterEventHook(QEnterEvent *e) {
	_hideTimer.cancel();
	setMouseTracking(true);
	setOver(true);
}

void ScrollBar::leaveEventHook(QEvent *e) {
	if (!_moving) {
		setMouseTracking(false);
	}
	setOver(false);
	setOverBar(false);
	if (_st->hiding && !_hiding) {
		_hideTimer.callOnce(_hideIn);
	}
}

void ScrollBar::mouseMoveEvent(QMouseEvent *e) {
	setOverBar(_bar.contains(e->pos()));
	if (_moving) {
		int delta = 0, barDelta = _vertical ? (area()->height() - _bar.height()) : (area()->width() - _bar.width());
		if (barDelta > 0) {
			QPoint d = (e->globalPos() - _dragStart);
			delta = int32((_vertical ? (d.y() * int64(area()->scrollTopMax())) : (d.x() * int64(area()->scrollLeftMax()))) / barDelta);
		}
		_connected->setValue(_startFrom + delta);
	}
}

void ScrollBar::mousePressEvent(QMouseEvent *e) {
	if (!width() || !height()) return;

	_dragStart = e->globalPos();
	area()->setMovingByScrollBar(true);
	setMoving(true);
	if (_overbar) {
		_startFrom = _connected->value();
	} else {
		int32 val = _vertical ? e->pos().y() : e->pos().x(), div = _vertical ? height() : width();
		val = (val <= _st->deltat) ? 0 : (val - _st->deltat);
		div = (div <= _st->deltat + _st->deltab) ? 1 : (div - _st->deltat - _st->deltab);
		_startFrom = _vertical ? int32((val * int64(area()->scrollTopMax())) / div) : ((val * int64(area()->scrollLeftMax())) / div);
		_connected->setValue(_startFrom);
		setOverBar(true);
	}
}

void ScrollBar::mouseReleaseEvent(QMouseEvent *e) {
	if (_moving) {
		area()->setMovingByScrollBar(false);
		setMoving(false);
	}
	if (!_over) {
		setMouseTracking(false);
	}
}

void ScrollBar::resizeEvent(QResizeEvent *e) {
	updateBar();
}

void ScrollBar::wheelEvent(QWheelEvent *e) {
	static_cast<ScrollArea*>(parentWidget())->viewportEvent(e);
}

auto ScrollBar::shadowVisibilityChanged() const
-> rpl::producer<ScrollBar::ShadowVisibility> {
	return _shadowVisibilityChanged.events();
}

ScrollArea::ScrollArea(
	QWidget *parent,
	const style::ScrollArea &st,
	bool handleTouch)
: Parent(parent)
, _st(st)
, _horizontalBar(this, false, &_st)
, _verticalBar(this, true, &_st)
, _topShadow(this, &_st)
, _bottomShadow(this, &_st)
, _touchEnabled(handleTouch) {
	setLayoutDirection(style::LayoutDirection());
	setFocusPolicy(Qt::NoFocus);

	_verticalBar->shadowVisibilityChanged(
	) | rpl::start_with_next([=](const ScrollBar::ShadowVisibility &data) {
		((data.type == ScrollShadow::Type::Top)
			? _topShadow
			: _bottomShadow)->changeVisibility(data.visible);
	}, lifetime());

	_verticalBar->updateBar(true);

	verticalScrollBar()->setSingleStep(style::ConvertScale(verticalScrollBar()->singleStep()));
	horizontalScrollBar()->setSingleStep(style::ConvertScale(horizontalScrollBar()->singleStep()));

	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	setFrameStyle(int(QFrame::NoFrame) | QFrame::Plain);
	viewport()->setAutoFillBackground(false);

	_horizontalValue = horizontalScrollBar()->value();
	_verticalValue = verticalScrollBar()->value();

	if (_touchEnabled) {
		viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
		_touchTimer.setCallback([=] { _touchRightButton = true; });
		_touchScrollTimer.setCallback([=] { touchScrollTimer(); });
	}
}

void ScrollArea::touchDeaccelerate(int32 elapsed) {
	int32 x = _touchSpeed.x();
	int32 y = _touchSpeed.y();
	_touchSpeed.setX((x == 0) ? x : (x > 0) ? qMax(0, x - elapsed) : qMin(0, x + elapsed));
	_touchSpeed.setY((y == 0) ? y : (y > 0) ? qMax(0, y - elapsed) : qMin(0, y + elapsed));
}

void ScrollArea::scrolled() {
	if (const auto inner = widget()) {
		SendPendingMoveResizeEvents(inner);
	}

	bool em = false;
	int horizontalValue = horizontalScrollBar()->value();
	int verticalValue = verticalScrollBar()->value();
	if (_horizontalValue != horizontalValue) {
		if (_disabled) {
			horizontalScrollBar()->setValue(_horizontalValue);
		} else {
			_horizontalValue = horizontalValue;
			if (_st.hiding) {
				_horizontalBar->hideTimeout(_st.hiding);
			}
			em = true;
		}
	}
	if (_verticalValue != verticalValue) {
		if (_disabled) {
			verticalScrollBar()->setValue(_verticalValue);
		} else {
			_verticalValue = verticalValue;
			if (_st.hiding) {
				_verticalBar->hideTimeout(_st.hiding);
			}
			em = true;
			_scrollTopUpdated.fire_copy(_verticalValue);
		}
	}
	if (em) {
		_scrolls.fire({});
		if (!_movingByScrollBar) {
			SendSynteticMouseEvent(this, QEvent::MouseMove, Qt::NoButton);
		}
	}
}

void ScrollArea::innerResized() {
	_innerResizes.fire({});
}

int ScrollArea::scrollWidth() const {
	QWidget *w(widget());
	return w ? qMax(w->width(), width()) : width();
}

int ScrollArea::scrollHeight() const {
	QWidget *w(widget());
	return w ? qMax(w->height(), height()) : height();
}

int ScrollArea::scrollLeftMax() const {
	return scrollWidth() - width();
}

int ScrollArea::scrollTopMax() const {
	return scrollHeight() - height();
}

int ScrollArea::scrollLeft() const {
	return _horizontalValue;
}

int ScrollArea::scrollTop() const {
	return _verticalValue;
}

void ScrollArea::touchScrollTimer() {
	auto nowTime = crl::now();
	if (_touchScrollState == TouchScrollState::Acceleration && _touchWaitingAcceleration && (nowTime - _touchAccelerationTime) > 40) {
		_touchScrollState = TouchScrollState::Manual;
		touchResetSpeed();
	} else if (_touchScrollState == TouchScrollState::Auto || _touchScrollState == TouchScrollState::Acceleration) {
		int32 elapsed = int32(nowTime - _touchTime);
		QPoint delta = _touchSpeed * elapsed / 1000;
		bool hasScrolled = touchScroll(delta);

		if (_touchSpeed.isNull() || !hasScrolled) {
			_touchScrollState = TouchScrollState::Manual;
			_touchScroll = false;
			_touchScrollTimer.cancel();
		} else {
			_touchTime = nowTime;
		}
		touchDeaccelerate(elapsed);
	}
}

void ScrollArea::touchUpdateSpeed() {
	const auto nowTime = crl::now();
	if (_touchPrevPosValid) {
		const int elapsed = nowTime - _touchSpeedTime;
		if (elapsed) {
			const QPoint newPixelDiff = (_touchPos - _touchPrevPos);
			const QPoint pixelsPerSecond = newPixelDiff * (1000 / elapsed);

			// fingers are inacurates, we ignore small changes to avoid stopping the autoscroll because
			// of a small horizontal offset when scrolling vertically
			const int newSpeedY = (qAbs(pixelsPerSecond.y()) > kFingerAccuracyThreshold) ? pixelsPerSecond.y() : 0;
			const int newSpeedX = (qAbs(pixelsPerSecond.x()) > kFingerAccuracyThreshold) ? pixelsPerSecond.x() : 0;
			if (_touchScrollState == TouchScrollState::Auto) {
				const int oldSpeedY = _touchSpeed.y();
				const int oldSpeedX = _touchSpeed.x();
				if ((oldSpeedY <= 0 && newSpeedY <= 0) || ((oldSpeedY >= 0 && newSpeedY >= 0)
					&& (oldSpeedX <= 0 && newSpeedX <= 0)) || (oldSpeedX >= 0 && newSpeedX >= 0)) {
					_touchSpeed.setY(std::clamp((oldSpeedY + (newSpeedY / 4)), -kMaxScrollAccelerated, +kMaxScrollAccelerated));
					_touchSpeed.setX(std::clamp((oldSpeedX + (newSpeedX / 4)), -kMaxScrollAccelerated, +kMaxScrollAccelerated));
				} else {
					_touchSpeed = QPoint();
				}
			} else {
				// we average the speed to avoid strange effects with the last delta
				if (!_touchSpeed.isNull()) {
					_touchSpeed.setX(std::clamp((_touchSpeed.x() / 4) + (newSpeedX * 3 / 4), -kMaxScrollFlick, +kMaxScrollFlick));
					_touchSpeed.setY(std::clamp((_touchSpeed.y() / 4) + (newSpeedY * 3 / 4), -kMaxScrollFlick, +kMaxScrollFlick));
				} else {
					_touchSpeed = QPoint(newSpeedX, newSpeedY);
				}
			}
		}
	} else {
		_touchPrevPosValid = true;
	}
	_touchSpeedTime = nowTime;
	_touchPrevPos = _touchPos;
}

void ScrollArea::touchResetSpeed() {
	_touchSpeed = QPoint();
	_touchPrevPosValid = false;
}

bool ScrollArea::eventHook(QEvent *e) {
	const auto was = (e->type() == QEvent::LayoutRequest)
		? verticalScrollBar()->minimum()
		: 0;
	const auto result = RpWidgetBase<QScrollArea>::eventHook(e);
	if (was) {
		// Because LayoutRequest resets custom-set minimum allowed value.
		verticalScrollBar()->setMinimum(was);
	}
	return result;
}

bool ScrollArea::eventFilter(QObject *obj, QEvent *e) {
	const auto result = QScrollArea::eventFilter(obj, e);
	return (obj == widget() && filterOutTouchEvent(e)) || result;
}

bool ScrollArea::viewportEvent(QEvent *e) {
	if (filterOutTouchEvent(e)) {
		return true;
	} else if (e->type() == QEvent::Wheel) {
		if (_customWheelProcess
			&& _customWheelProcess(static_cast<QWheelEvent*>(e))) {
			return true;
		}
	}
	return QScrollArea::viewportEvent(e);
}

bool ScrollArea::filterOutTouchEvent(QEvent *e) {
	const auto type = e->type();
	if (type == QEvent::TouchBegin
		|| type == QEvent::TouchUpdate
		|| type == QEvent::TouchEnd
		|| type == QEvent::TouchCancel) {
		const auto ev = static_cast<QTouchEvent*>(e);
		if (ev->device()->type() == base::TouchDevice::TouchScreen) {
			if (_customTouchProcess && _customTouchProcess(ev)) {
				return true;
			} else if (_touchEnabled) {
				touchEvent(ev);
				return true;
			}
		}
	}
	return false;
}

void ScrollArea::touchEvent(QTouchEvent *e) {
	if (!e->touchPoints().isEmpty()) {
		_touchPrevPos = _touchPos;
		_touchPos = e->touchPoints().cbegin()->screenPos().toPoint();
	}

	switch (e->type()) {
	case QEvent::TouchBegin: {
		if (_touchPress || e->touchPoints().isEmpty()) return;
		_touchPress = true;
		if (_touchScrollState == TouchScrollState::Auto) {
			_touchScrollState = TouchScrollState::Acceleration;
			_touchWaitingAcceleration = true;
			_touchMaybePressing = false;
			_touchAccelerationTime = crl::now();
			touchUpdateSpeed();
			_touchStart = _touchPos;
		} else {
			_touchScroll = false;
			_touchMaybePressing = true;
			_touchTimer.callOnce(QApplication::startDragTime());
		}
		_touchStart = _touchPrevPos = _touchPos;
		_touchRightButton = false;
	} break;

	case QEvent::TouchUpdate: {
		if (!_touchPress) return;
		if (!_touchScroll && (_touchPos - _touchStart).manhattanLength() >= QApplication::startDragDistance()) {
			_touchTimer.cancel();
			_touchScroll = true;
			_touchMaybePressing = false;
			touchUpdateSpeed();
		}
		if (_touchScroll) {
			if (_touchScrollState == TouchScrollState::Manual) {
				touchScrollUpdated(_touchPos);
			} else if (_touchScrollState == TouchScrollState::Acceleration) {
				touchUpdateSpeed();
				_touchAccelerationTime = crl::now();
				if (_touchSpeed.isNull()) {
					_touchScrollState = TouchScrollState::Manual;
				}
			}
		}
	} break;

	case QEvent::TouchEnd: {
		if (!_touchPress) return;
		_touchPress = false;
		auto weak = base::make_weak(this);
		if (_touchScroll) {
			if (_touchScrollState == TouchScrollState::Manual) {
				_touchScrollState = TouchScrollState::Auto;
				_touchPrevPosValid = false;
				_touchScrollTimer.callEach(15);
				_touchTime = crl::now();
			} else if (_touchScrollState == TouchScrollState::Auto) {
				_touchScrollState = TouchScrollState::Manual;
				_touchScroll = false;
				touchResetSpeed();
			} else if (_touchScrollState == TouchScrollState::Acceleration) {
				_touchScrollState = TouchScrollState::Auto;
				_touchWaitingAcceleration = false;
				_touchPrevPosValid = false;
			}
		} else if (window()) { // one short tap -- like left mouse click, one long tap -- like right mouse click
			Qt::MouseButton btn(_touchRightButton ? Qt::RightButton : Qt::LeftButton);

			if (weak) SendSynteticMouseEvent(this, QEvent::MouseMove, Qt::NoButton, _touchStart);
			if (weak) SendSynteticMouseEvent(this, QEvent::MouseButtonPress, btn, _touchStart);
			if (weak) SendSynteticMouseEvent(this, QEvent::MouseButtonRelease, btn, _touchStart);

			if (weak && _touchRightButton) {
				auto windowHandle = window()->windowHandle();
				auto localPoint = windowHandle->mapFromGlobal(_touchStart);
				QContextMenuEvent ev(QContextMenuEvent::Mouse, localPoint, _touchStart, QGuiApplication::keyboardModifiers());
				ev.setTimestamp(crl::now());
				QGuiApplication::sendEvent(windowHandle, &ev);
			}
		}
		if (weak) {
			_touchTimer.cancel();
			_touchRightButton = false;
			_touchMaybePressing = false;
		}
	} break;

	case QEvent::TouchCancel: {
		_touchPress = false;
		_touchScroll = false;
		_touchMaybePressing = false;
		_touchScrollState = TouchScrollState::Manual;
		_touchTimer.cancel();
	} break;
	}
}

void ScrollArea::touchScrollUpdated(const QPoint &screenPos) {
	_touchPos = screenPos;
	touchScroll(_touchPos - _touchPrevPos);
	touchUpdateSpeed();
}

void ScrollArea::disableScroll(bool dis) {
	_disabled = dis;
	if (_disabled && _st.hiding) {
		_horizontalBar->hideTimeout(0);
		_verticalBar->hideTimeout(0);
	}
}

void ScrollArea::scrollContentsBy(int dx, int dy) {
	if (_disabled) {
		return;
	}
	QScrollArea::scrollContentsBy(dx, dy);
}

bool ScrollArea::touchScroll(const QPoint &delta) {
	const auto top = scrollTop();
	const auto topMax = scrollTopMax();
	const auto left = scrollLeft();
	const auto leftMax = scrollLeftMax();
	const auto xAbs = qAbs(delta.x());
	const auto yAbs = qAbs(delta.y());
	const auto direction = (leftMax <= 0 || yAbs > xAbs)
		? Qt::Vertical
		: Qt::Horizontal;
	const auto was = (direction == Qt::Vertical) ? top : left;
	const auto now = (direction == Qt::Vertical)
		? std::clamp(top - delta.y(), 0, topMax)
		: std::clamp(left - delta.x(), 0, leftMax);
	if (now == was) {
		return false;
	} else if (direction == Qt::Vertical) {
		scrollToY(now);
	} else {
		horizontalScrollBar()->setValue(now);
	}
	return true;
}

void ScrollArea::resizeEvent(QResizeEvent *e) {
	QScrollArea::resizeEvent(e);
	_horizontalBar->recountSize();
	_verticalBar->recountSize();
	_topShadow->setGeometry(QRect(0, 0, width(), qAbs(_st.topsh)));
	_bottomShadow->setGeometry(QRect(0, height() - qAbs(_st.bottomsh), width(), qAbs(_st.bottomsh)));
	_geometryChanged.fire({});
}

void ScrollArea::moveEvent(QMoveEvent *e) {
	QScrollArea::moveEvent(e);
	_geometryChanged.fire({});
}

void ScrollArea::keyPressEvent(QKeyEvent *e) {
	if ((e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)
		&& (e->modifiers().testFlag(Qt::AltModifier)
			|| e->modifiers().testFlag(Qt::ControlModifier))) {
		e->ignore();
	} else if(e->key() == Qt::Key_Escape || e->key() == Qt::Key_Back) {
		((QObject*)widget())->event(e);
	} else {
		QScrollArea::keyPressEvent(e);
	}
}

void ScrollArea::enterEventHook(QEnterEvent *e) {
	if (_disabled) return;
	if (_st.hiding) {
		_horizontalBar->hideTimeout(_st.hiding);
		_verticalBar->hideTimeout(_st.hiding);
	}
	return QScrollArea::enterEvent(e);
}

void ScrollArea::leaveEventHook(QEvent *e) {
	if (_st.hiding) {
		_horizontalBar->hideTimeout(0);
		_verticalBar->hideTimeout(0);
	}
	return QScrollArea::leaveEvent(e);
}

void ScrollArea::scrollTo(ScrollToRequest request) {
	scrollToY(request.ymin, request.ymax);
}

void ScrollArea::scrollToWidget(not_null<QWidget*> widget) {
	if (auto local = this->widget()) {
		auto globalPosition = widget->mapToGlobal(QPoint(0, 0));
		auto localPosition = local->mapFromGlobal(globalPosition);
		auto localTop = localPosition.y();
		auto localBottom = localTop + widget->height();
		scrollToY(localTop, localBottom);
	}
}

int ScrollArea::computeScrollToX(int toLeft, int toRight) {
	if (const auto inner = widget()) {
		SendPendingMoveResizeEvents(inner);
	}
	SendPendingMoveResizeEvents(this);
	return ComputeScrollTo(
		toLeft,
		toRight,
		0,
		scrollLeftMax(),
		scrollLeft(),
		width());
}

int ScrollArea::computeScrollToY(int toTop, int toBottom) {
	if (const auto inner = widget()) {
		SendPendingMoveResizeEvents(inner);
	}
	SendPendingMoveResizeEvents(this);
	return ComputeScrollTo(
		toTop,
		toBottom,
		0,
		scrollTopMax(),
		scrollTop(),
		height());
}

void ScrollArea::scrollToX(int toLeft, int toRight) {
	horizontalScrollBar()->setValue(computeScrollToX(toLeft, toRight));
}

void ScrollArea::scrollToY(int toTop, int toBottom) {
	verticalScrollBar()->setValue(computeScrollToY(toTop, toBottom));
}

void ScrollArea::doSetOwnedWidget(object_ptr<QWidget> w) {
	if (widget() && _touchEnabled) {
		widget()->removeEventFilter(this);
		if (!_widgetAcceptsTouch) widget()->setAttribute(Qt::WA_AcceptTouchEvents, false);
	}
	_widget = std::move(w);
	QScrollArea::setWidget(_widget);
	if (_widget) {
		_widget->setAutoFillBackground(false);
		if (_touchEnabled) {
			_widget->installEventFilter(this);
			_widgetAcceptsTouch = _widget->testAttribute(Qt::WA_AcceptTouchEvents);
			_widget->setAttribute(Qt::WA_AcceptTouchEvents);
		}
	}
}

object_ptr<QWidget> ScrollArea::doTakeWidget() {
	QScrollArea::takeWidget();
	return std::move(_widget);
}

void ScrollArea::rangeChanged(int oldMax, int newMax, bool vertical) {
}

void ScrollArea::updateBars() {
	_horizontalBar->updateBar(true);
	_verticalBar->updateBar(true);
}

bool ScrollArea::focusNextPrevChild(bool next) {
	if (QWidget::focusNextPrevChild(next)) {
//		if (QWidget *fw = focusWidget())
//			ensureWidgetVisible(fw);
		return true;
	}
	return false;
}

void ScrollArea::setMovingByScrollBar(bool movingByScrollBar) {
	_movingByScrollBar = movingByScrollBar;
}

rpl::producer<> ScrollArea::scrolls() const {
	return _scrolls.events();
}

rpl::producer<> ScrollArea::innerResizes() const {
	return _innerResizes.events();
}

rpl::producer<> ScrollArea::geometryChanged() const {
	return _geometryChanged.events();
}

rpl::producer<bool> ScrollArea::touchMaybePressing() const {
	return _touchMaybePressing.value();
}

} // namespace Ui
