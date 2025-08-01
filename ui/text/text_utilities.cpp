// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/text/text_utilities.h"

#include "base/algorithm.h"
#include "base/qt/qt_string_view.h"
#include "ui/text/custom_emoji_instance.h"
#include "ui/text/text_custom_emoji.h"
#include "styles/style_basic.h"

#include <QtCore/QRegularExpression>

namespace Ui {
namespace Text {
namespace {

struct IconEmojiData {
	base::flat_map<not_null<const style::IconEmoji*>, int> indices;
	std::vector<not_null<const style::IconEmoji*>> list;
};

[[nodiscard]] TextWithEntities WithSingleEntity(
		const QString &text,
		EntityType type,
		const QString &data = QString()) {
	auto result = TextWithEntities{ text };
	result.entities.push_back({ type, 0, int(text.size()), data });
	return result;
}

[[nodiscard]] IconEmojiData &IconEmojiInfo() {
	static IconEmojiData result;
	return result;
}

[[nodiscard]] QString IconEmojiPrefix() {
	return u"icon-emoji-"_q;
}

class IconEmojiObject final : public CustomEmoji {
public:
	explicit IconEmojiObject(not_null<const style::IconEmoji*> emoji);

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	const not_null<const style::IconEmoji*> _emoji;
	Ui::CustomEmoji::IconEmojiFrameCache _cache;

};

IconEmojiObject::IconEmojiObject(not_null<const style::IconEmoji*> emoji)
: _emoji(emoji) {
}

int IconEmojiObject::width() {
	return _emoji->padding.left()
		+ _emoji->icon.width()
		+ _emoji->padding.right();
}

QString IconEmojiObject::entityData() {
	return IconEmojiPrefix()
		+ QString::number(IconEmojiInfo().indices[_emoji]);
}

void IconEmojiObject::paint(QPainter &p, const Context &context) {
	Ui::CustomEmoji::PaintIconEmoji(p, context, _emoji, _cache);
}

void IconEmojiObject::unload() {
}

bool IconEmojiObject::ready() {
	return true;
}

bool IconEmojiObject::readyInDefaultState() {
	return true;
}

} // namespace

TextWithEntities Bold(const QString &text) {
	return WithSingleEntity(text, EntityType::Bold);
}

TextWithEntities Semibold(const QString &text) {
	return WithSingleEntity(text, EntityType::Semibold);
}

TextWithEntities Italic(const QString &text) {
	return WithSingleEntity(text, EntityType::Italic);
}

TextWithEntities Link(const QString &text, const QString &url) {
	return WithSingleEntity(text, EntityType::CustomUrl, url);
}

TextWithEntities Link(const QString &text, int index) {
	return Link(text, u"internal:index"_q + QChar(index));
}

TextWithEntities Link(TextWithEntities text, const QString &url) {
	return Wrapped(std::move(text), EntityType::CustomUrl, url);
}

TextWithEntities Link(TextWithEntities text, int index) {
	return Link(std::move(text), u"internal:index"_q + QChar(index));
}

TextWithEntities Colorized(const QString &text, int index) {
	const auto data = index ? QString(QChar(index)) : QString();
	return WithSingleEntity(text, EntityType::Colorized, data);
}

TextWithEntities Colorized(TextWithEntities text, int index) {
	const auto data = index ? QString(QChar(index)) : QString();
	return Wrapped(std::move(text), EntityType::Colorized, data);
}

TextWithEntities Wrapped(
		TextWithEntities text,
		EntityType type,
		const QString &data) {
	text.entities.insert(
		text.entities.begin(),
		{ type, 0, int(text.text.size()), data });
	return text;
}

TextWithEntities RichLangValue(const QString &text) {
	static const auto kStart = QRegularExpression("(\\*\\*|__)");

	auto result = TextWithEntities();
	auto offset = 0;
	while (offset < text.size()) {
		const auto m = kStart.match(text, offset);
		if (!m.hasMatch()) {
			result.text.append(base::StringViewMid(text, offset));
			break;
		}
		const auto position = m.capturedStart();
		const auto from = m.capturedEnd();
		const auto tag = m.capturedView();
		const auto till = text.indexOf(tag, from + 1);
		if (till <= from) {
			offset = from;
			continue;
		}
		if (position > offset) {
			result.text.append(base::StringViewMid(text, offset, position - offset));
		}
		const auto type = (tag == qstr("__"))
			? EntityType::Italic
			: EntityType::Bold;
		result.entities.push_back({ type, int(result.text.size()), int(till - from) });
		result.text.append(base::StringViewMid(text, from, till - from));
		offset = till + tag.size();
	}
	return result;
}

TextWithEntities SingleCustomEmoji(QString data, QString text) {
	if (text.isEmpty()) {
		text = u"@"_q;
	}
	return {
		text,
		{ EntityInText(EntityType::CustomEmoji, 0, text.size(), data)},
	};
}

TextWithEntities IconEmoji(
		not_null<const style::IconEmoji*> emoji,
		QString text) {
	const auto index = [&] {
		auto &info = IconEmojiInfo();
		const auto count = int(info.list.size());
		auto i = info.indices.emplace(emoji, count).first;
		if (i->second == count) {
			info.list.push_back(emoji);
		}
		return i->second;
	}();
	return SingleCustomEmoji(
		IconEmojiPrefix() + QString::number(index),
		text);
}

std::unique_ptr<CustomEmoji> TryMakeSimpleEmoji(QStringView data) {
	const auto prefix = IconEmojiPrefix();
	if (!data.startsWith(prefix)) {
		return nullptr;
	}
	auto &info = IconEmojiInfo();
	const auto index = data.mid(prefix.size()).toInt();
	return (index >= 0 && index < info.list.size())
		? std::make_unique<IconEmojiObject>(info.list[index])
		: nullptr;
}

TextWithEntities Mid(const TextWithEntities &text, int position, int n) {
	if (n == -1) {
		n = int(text.text.size()) - position;
	}
	const auto midEnd = (position + n);
	auto entities = ranges::views::all(
		text.entities
	) | ranges::views::filter([&](const EntityInText &entity) {
		// Intersects of ranges.
		const auto l1 = entity.offset();
		const auto r1 = entity.offset() + entity.length() - 1;
		const auto l2 = position;
		const auto r2 = midEnd - 1;
		return !(l1 > r2 || l2 > r1);
	}) | ranges::views::transform([&](const EntityInText &entity) {
		if ((entity.offset() == position) && (entity.length() == n)) {
			return entity;
		}
		const auto start = std::max(entity.offset(), position);
		const auto end = std::min(entity.offset() + entity.length(), midEnd);
		return EntityInText(
			entity.type(),
			start - position,
			end - start,
			entity.data());
	}) | ranges::to<EntitiesInText>();
	return {
		.text = text.text.mid(position, n),
		.entities = std::move(entities),
	};
}

TextWithEntities Filtered(
		const TextWithEntities &text,
		const std::vector<EntityType> &types) {
	auto result = ranges::views::all(
		text.entities
	) | ranges::views::filter([&](const EntityInText &entity) {
		return ranges::contains(types, entity.type());
	}) | ranges::to<EntitiesInText>();
	return { .text = text.text, .entities = std::move(result) };
}

QString FixAmpersandInAction(QString text) {
	return text.replace('&', u"&&"_q);
}

TextWithEntities WrapEmailPattern(const QString &pattern) {
	constexpr auto kHidden = '*';
	const auto from = int(pattern.indexOf(kHidden));
	const auto to = int(pattern.lastIndexOf(kHidden));

	if (from != -1 && to != -1 && from <= to) {
		const auto length = to - from + 1;
		auto result = TextWithEntities{ pattern };
		result.entities.push_back({ EntityType::Spoiler, from, length });
		return result;
	}
	return { pattern };
}

} // namespace Text
} // namespace Ui
