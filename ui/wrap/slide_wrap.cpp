// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/wrap/slide_wrap.h"

#include "ui/ui_utility.h"
#include "styles/style_basic.h"

#include <rpl/combine.h>
#include <range/v3/algorithm/find.hpp>

namespace Ui {

SlideWrap<RpWidget>::SlideWrap(
	QWidget *parent,
	object_ptr<RpWidget> &&child)
: SlideWrap(
	parent,
	std::move(child),
	style::margins()) {
}

SlideWrap<RpWidget>::SlideWrap(
	QWidget *parent,
	const style::margins &padding)
: SlideWrap(parent, nullptr, padding) {
}

SlideWrap<RpWidget>::SlideWrap(
	QWidget *parent,
	object_ptr<RpWidget> &&child,
	const style::margins &padding)
: Parent(
	parent,
	object_ptr<PaddingWrap<RpWidget>>(
		parent,
		std::move(child),
		padding))
, _duration(st::slideWrapDuration) {
}

SlideWrap<RpWidget> *SlideWrap<RpWidget>::setDuration(int duration) {
	_duration = duration;
	return this;
}

SlideWrap<RpWidget> *SlideWrap<RpWidget>::setDirectionUp(bool up) {
	_up = up;
	return this;
}

SlideWrap<RpWidget> *SlideWrap<RpWidget>::toggle(
		bool shown,
		anim::type animated) {
	auto animate = (animated == anim::type::normal) && _duration;
	auto changed = (_toggled != shown);
	if (changed) {
		_toggled = shown;
		if (animate) {
			_animation.start(
				[this] { animationStep(); },
				_toggled ? 0. : 1.,
				_toggled ? 1. : 0.,
				_duration,
				anim::linear);
		}
	}
	if (animate) {
		animationStep();
	} else {
		finishAnimating();
	}
	if (changed) {
		_toggledChanged.fire_copy(_toggled);
	}
	return this;
}

SlideWrap<RpWidget> *SlideWrap<RpWidget>::finishAnimating() {
	_animation.stop();
	animationStep();
	return this;
}

SlideWrap<RpWidget> *SlideWrap<RpWidget>::toggleOn(
		rpl::producer<bool> &&shown,
		anim::type animated) {
	std::move(
		shown
	) | rpl::start_with_next([=](bool shown) {
		toggle(shown, animated);
	}, lifetime());
	finishAnimating();
	return this;
}

void SlideWrap<RpWidget>::setMinimalHeight(int height) {
	_minimalHeight = height;
}

void SlideWrap<RpWidget>::animationStep() {
	const auto weak = wrapped();
	if (weak && !_up) {
		const auto margins = getMargins();
		weak->moveToLeft(margins.left(), margins.top());
	}
	const auto newWidth = weak ? weak->width() : width();
	const auto current = _animation.value(_toggled ? 1. : 0.);
	const auto newHeight = weak
		? (_animation.animating()
			? anim::interpolate(
				_minimalHeight,
				weak->heightNoMargins(),
				current)
			: (_toggled ? weak->height() : _minimalHeight))
		: 0;
	if (weak && _up) {
		const auto margins = getMargins();
		weak->moveToLeft(
			margins.left(),
			margins.top() - (weak->height() - newHeight));
	}
	if (newWidth != width() || newHeight != height()) {
		resize(newWidth, newHeight);
	}
	const auto shouldBeHidden = !_toggled && !_animation.animating();
	if (shouldBeHidden != isHidden()) {
		const auto guard = base::make_weak(this);
		setVisible(!shouldBeHidden || _minimalHeight);
		if (shouldBeHidden && guard) {
			SendPendingMoveResizeEvents(this);
		}
	}
}

QMargins SlideWrap<RpWidget>::getMargins() const {
	auto result = wrapped()->getMargins();
	return (animating() || !_toggled)
		? QMargins(result.left(), 0, result.right(), 0)
		: result;
}

int SlideWrap<RpWidget>::resizeGetHeight(int newWidth) {
	if (wrapped()) {
		wrapped()->resizeToWidth(newWidth);
	}
	return heightNoMargins();
}

void SlideWrap<RpWidget>::wrappedSizeUpdated(QSize size) {
	if (_animation.animating()) {
		animationStep();
	} else if (_toggled) {
		resize(size);
	}
}

rpl::producer<bool> MultiSlideTracker::atLeastOneShownValue() const {
	if (_widgets.empty()) {
		return rpl::single(false);
	}
	auto shown = std::vector<rpl::producer<bool>>();
	shown.reserve(_widgets.size());
	for (auto &widget : _widgets) {
		shown.push_back(widget->toggledValue());
	}
	return rpl::combine(
		std::move(shown),
		[](const std::vector<bool> &values) {
			return ranges::find(values, true) != values.end();
		});
}

} // namespace Ui

