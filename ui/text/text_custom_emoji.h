// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtCore/QSize>
#include <QtCore/QPoint>

#include <crl/crl_time.h>

#include <any>

class QPainter;

namespace Ui::Text {

struct MarkedContext;

[[nodiscard]] int AdjustCustomEmojiSize(int emojiSize);

struct CustomEmojiPaintContext {
	required<QColor> textColor;
	QSize size; // Required only when scaled = true, for path scaling.
	crl::time now = 0;
	float64 scale = 0.;
	QPoint position;
	bool paused = false;
	bool scaled = false;

	mutable struct {
		bool colorized = false;
		bool forceFirstFrame = false;
		bool forceLastFrame = false;
		bool overrideFirstWithLastFrame = false;
	} internal;
};

class CustomEmoji {
public:
	virtual ~CustomEmoji() = default;

	[[nodiscard]] virtual int width() = 0;
	[[nodiscard]] virtual QString entityData() = 0;

	using Context = CustomEmojiPaintContext;
	virtual void paint(QPainter &p, const Context &context) = 0;
	virtual void unload() = 0;
	[[nodiscard]] virtual bool ready() = 0;
	[[nodiscard]] virtual bool readyInDefaultState() = 0;

};

class ShiftedEmoji final : public CustomEmoji {
public:
	ShiftedEmoji(std::unique_ptr<CustomEmoji> wrapped, QPoint shift);

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	const std::unique_ptr<Ui::Text::CustomEmoji> _wrapped;
	const QPoint _shift;

};

class FirstFrameEmoji final : public CustomEmoji {
public:
	explicit FirstFrameEmoji(std::unique_ptr<CustomEmoji> wrapped);

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	const std::unique_ptr<Ui::Text::CustomEmoji> _wrapped;

};

class LimitedLoopsEmoji final : public CustomEmoji {
public:
	LimitedLoopsEmoji(
		std::unique_ptr<CustomEmoji> wrapped,
		int limit,
		bool stopOnLast = false);

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	const std::unique_ptr<Ui::Text::CustomEmoji> _wrapped;
	const int _limit = 0;
	int _played = 0;
	bool _inLoop = false;
	bool _stopOnLast = false;

};

class StaticCustomEmoji final : public CustomEmoji {
public:
	StaticCustomEmoji(
		QImage &&image,
		QString entity,
		QMargins padding = {});

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	QImage _image;
	QString _entity;
	QMargins _padding;

};

class PaletteDependentCustomEmoji final : public CustomEmoji {
public:
	PaletteDependentCustomEmoji(
		Fn<QImage()> factory,
		QString entity,
		QMargins padding = {});

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	void validateFrame();

	Fn<QImage()> _factory;
	QString _entity;
	QMargins _padding;
	QImage _frame;
	int _paletteVersion = 0;

};

[[nodiscard]] std::unique_ptr<CustomEmoji> MakeCustomEmoji(
	QStringView data,
	const MarkedContext &context);

} // namespace Ui::Text
