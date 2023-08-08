/*
 * Copyright (c) 2022-2023, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "GeneratorUtil.h"
#include <AK/SourceGenerator.h>
#include <AK/StringBuilder.h>
#include <LibCore/ArgsParser.h>
#include <LibMain/Main.h>

ErrorOr<void> generate_header_file(JsonObject& media_feature_data, Core::File& file);
ErrorOr<void> generate_implementation_file(JsonObject& media_feature_data, Core::File& file);

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    StringView generated_header_path;
    StringView generated_implementation_path;
    StringView media_features_json_path;

    Core::ArgsParser args_parser;
    args_parser.add_option(generated_header_path, "Path to the MediaFeatureID header file to generate", "generated-header-path", 'h', "generated-header-path");
    args_parser.add_option(generated_implementation_path, "Path to the MediaFeatureID implementation file to generate", "generated-implementation-path", 'c', "generated-implementation-path");
    args_parser.add_option(media_features_json_path, "Path to the JSON file to read from", "json-path", 'j', "json-path");
    args_parser.parse(arguments);

    auto json = TRY(read_entire_file_as_json(media_features_json_path));
    VERIFY(json.is_object());
    auto media_feature_data = json.as_object();

    auto generated_header_file = TRY(Core::File::open(generated_header_path, Core::File::OpenMode::Write));
    auto generated_implementation_file = TRY(Core::File::open(generated_implementation_path, Core::File::OpenMode::Write));

    TRY(generate_header_file(media_feature_data, *generated_header_file));
    TRY(generate_implementation_file(media_feature_data, *generated_implementation_file));

    return 0;
}

ErrorOr<void> generate_header_file(JsonObject& media_feature_data, Core::File& file)
{
    StringBuilder builder;
    SourceGenerator generator { builder };
    TRY(generator.try_append(R"~~~(#pragma once

#include <AK/StringView.h>
#include <AK/Traits.h>
#include <LibWeb/CSS/ValueID.h>

namespace Web::CSS {

enum class MediaFeatureValueType {
    Boolean,
    Integer,
    Length,
    Ratio,
    Resolution,
};

enum class MediaFeatureID {)~~~"));

    TRY(media_feature_data.try_for_each_member([&](auto& name, auto&) -> ErrorOr<void> {
        auto member_generator = TRY(generator.fork());
        TRY(member_generator.set("name:titlecase", TRY(title_casify(name))));
        TRY(member_generator.try_append(R"~~~(
    @name:titlecase@,)~~~"));
        return {};
    }));

    TRY(generator.try_append(R"~~~(
};

Optional<MediaFeatureID> media_feature_id_from_string(StringView);
StringView string_from_media_feature_id(MediaFeatureID);

bool media_feature_type_is_range(MediaFeatureID);
bool media_feature_accepts_type(MediaFeatureID, MediaFeatureValueType);
bool media_feature_accepts_identifier(MediaFeatureID, ValueID);

}
)~~~"));

    TRY(file.write_until_depleted(generator.as_string_view().bytes()));
    return {};
}

ErrorOr<void> generate_implementation_file(JsonObject& media_feature_data, Core::File& file)
{
    StringBuilder builder;
    SourceGenerator generator { builder };
    TRY(generator.try_append(R"~~~(
#include <LibWeb/CSS/MediaFeatureID.h>
#include <LibWeb/Infra/Strings.h>

namespace Web::CSS {

Optional<MediaFeatureID> media_feature_id_from_string(StringView string)
{)~~~"));

    TRY(media_feature_data.try_for_each_member([&](auto& name, auto&) -> ErrorOr<void> {
        auto member_generator = TRY(generator.fork());
        TRY(member_generator.set("name", TRY(String::from_deprecated_string(name))));
        TRY(member_generator.set("name:titlecase", TRY(title_casify(name))));
        TRY(member_generator.try_append(R"~~~(
    if (Infra::is_ascii_case_insensitive_match(string, "@name@"sv))
        return MediaFeatureID::@name:titlecase@;
)~~~"));
        return {};
    }));

    generator.append(R"~~~(
    return {};
}

StringView string_from_media_feature_id(MediaFeatureID media_feature_id)
{
    switch (media_feature_id) {)~~~");

    TRY(media_feature_data.try_for_each_member([&](auto& name, auto&) -> ErrorOr<void> {
        auto member_generator = TRY(generator.fork());
        TRY(member_generator.set("name", TRY(String::from_deprecated_string(name))));
        TRY(member_generator.set("name:titlecase", TRY(title_casify(name))));
        TRY(member_generator.try_append(R"~~~(
    case MediaFeatureID::@name:titlecase@:
        return "@name@"sv;)~~~"));
        return {};
    }));

    TRY(generator.try_append(R"~~~(
    }
    VERIFY_NOT_REACHED();
}

bool media_feature_type_is_range(MediaFeatureID media_feature_id)
{
    switch (media_feature_id) {)~~~"));

    TRY(media_feature_data.try_for_each_member([&](auto& name, auto& value) -> ErrorOr<void> {
        VERIFY(value.is_object());
        auto& feature = value.as_object();

        auto member_generator = TRY(generator.fork());
        TRY(member_generator.set("name:titlecase", TRY(title_casify(name))));
        VERIFY(feature.has("type"sv));
        auto feature_type = feature.get_deprecated_string("type"sv);
        VERIFY(feature_type.has_value());
        TRY(member_generator.set("is_range", feature_type.value() == "range" ? "true"_string : "false"_string));
        TRY(member_generator.try_append(R"~~~(
    case MediaFeatureID::@name:titlecase@:
        return @is_range@;)~~~"));
        return {};
    }));

    TRY(generator.try_append(R"~~~(
    }
    VERIFY_NOT_REACHED();
}

bool media_feature_accepts_type(MediaFeatureID media_feature_id, MediaFeatureValueType value_type)
{
    switch (media_feature_id) {)~~~"));

    TRY(media_feature_data.try_for_each_member([&](auto& name, auto& member) -> ErrorOr<void> {
        VERIFY(member.is_object());
        auto& feature = member.as_object();

        auto member_generator = TRY(generator.fork());
        TRY(member_generator.set("name:titlecase", TRY(title_casify(name))));
        TRY(member_generator.try_append(R"~~~(
    case MediaFeatureID::@name:titlecase@:)~~~"));

        bool have_output_value_type_switch = false;
        if (feature.has("values"sv)) {
            auto append_value_type_switch_if_needed = [&]() -> ErrorOr<void> {
                if (!have_output_value_type_switch) {
                    TRY(member_generator.try_append(R"~~~(
        switch (value_type) {)~~~"));
                }
                have_output_value_type_switch = true;
                return {};
            };
            auto values = feature.get_array("values"sv);
            VERIFY(values.has_value());
            auto& values_array = values.value();
            for (auto& type : values_array.values()) {
                VERIFY(type.is_string());
                auto type_name = type.as_string();
                // Skip identifiers.
                if (type_name[0] != '<')
                    continue;
                if (type_name == "<mq-boolean>") {
                    TRY(append_value_type_switch_if_needed());
                    TRY(member_generator.try_append(R"~~~(
        case MediaFeatureValueType::Boolean:
            return true;)~~~"));
                } else if (type_name == "<integer>") {
                    TRY(append_value_type_switch_if_needed());
                    TRY(member_generator.try_append(R"~~~(
        case MediaFeatureValueType::Integer:
            return true;)~~~"));
                } else if (type_name == "<length>") {
                    TRY(append_value_type_switch_if_needed());
                    TRY(member_generator.try_append(R"~~~(
        case MediaFeatureValueType::Length:
            return true;)~~~"));
                } else if (type_name == "<ratio>") {
                    TRY(append_value_type_switch_if_needed());
                    TRY(member_generator.try_append(R"~~~(
        case MediaFeatureValueType::Ratio:
            return true;)~~~"));
                } else if (type_name == "<resolution>") {
                    TRY(append_value_type_switch_if_needed());
                    TRY(member_generator.try_append(R"~~~(
        case MediaFeatureValueType::Resolution:
            return true;)~~~"));
                } else {
                    warnln("Unrecognized media-feature value type: `{}`", type_name);
                    VERIFY_NOT_REACHED();
                }
            }
        }
        if (have_output_value_type_switch) {
            TRY(member_generator.try_append(R"~~~(
        default:
            return false;
        })~~~"));
        } else {
            TRY(member_generator.try_append(R"~~~(
        return false;)~~~"));
        }
        return {};
    }));

    TRY(generator.try_append(R"~~~(
    }
    VERIFY_NOT_REACHED();
}

bool media_feature_accepts_identifier(MediaFeatureID media_feature_id, ValueID identifier)
{
    switch (media_feature_id) {)~~~"));

    TRY(media_feature_data.try_for_each_member([&](auto& name, auto& member) -> ErrorOr<void> {
        VERIFY(member.is_object());
        auto& feature = member.as_object();

        auto member_generator = TRY(generator.fork());
        TRY(member_generator.set("name:titlecase", TRY(title_casify(name))));
        TRY(member_generator.try_append(R"~~~(
    case MediaFeatureID::@name:titlecase@:)~~~"));

        bool have_output_identifier_switch = false;
        if (feature.has("values"sv)) {
            auto append_identifier_switch_if_needed = [&]() -> ErrorOr<void> {
                if (!have_output_identifier_switch) {
                    TRY(member_generator.try_append(R"~~~(
        switch (identifier) {)~~~"));
                }
                have_output_identifier_switch = true;
                return {};
            };
            auto values = feature.get_array("values"sv);
            VERIFY(values.has_value());
            auto& values_array = values.value();
            for (auto& identifier : values_array.values()) {
                VERIFY(identifier.is_string());
                auto identifier_name = identifier.as_string();
                // Skip types.
                if (identifier_name[0] == '<')
                    continue;
                TRY(append_identifier_switch_if_needed());

                auto ident_generator = TRY(member_generator.fork());
                TRY(ident_generator.set("identifier:titlecase", TRY(title_casify(identifier_name))));
                TRY(ident_generator.try_append(R"~~~(
        case ValueID::@identifier:titlecase@:
            return true;)~~~"));
            }
        }
        if (have_output_identifier_switch) {
            TRY(member_generator.try_append(R"~~~(
        default:
            return false;
        })~~~"));
        } else {
            TRY(member_generator.try_append(R"~~~(
        return false;)~~~"));
        }
        return {};
    }));

    TRY(generator.try_append(R"~~~(
    }
    VERIFY_NOT_REACHED();
}

}
)~~~"));

    TRY(file.write_until_depleted(generator.as_string_view().bytes()));
    return {};
}
