/*
 * Copyright (c) 2021, Tim Flynn <trflynn89@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/TypeCasts.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Intl/AbstractOperations.h>
#include <LibJS/Runtime/Intl/DisplayNames.h>
#include <LibJS/Runtime/Intl/DisplayNamesPrototype.h>
#include <LibUnicode/Locale.h>

namespace JS::Intl {

static DisplayNames* typed_this(GlobalObject& global_object)
{
    auto& vm = global_object.vm();

    auto* this_object = vm.this_value(global_object).to_object(global_object);
    if (!this_object)
        return nullptr;

    if (!is<DisplayNames>(this_object)) {
        vm.throw_exception<TypeError>(global_object, ErrorType::NotA, "Intl.DisplayNames");
        return nullptr;
    }

    return static_cast<DisplayNames*>(this_object);
}

// 12.4 Properties of the Intl.DisplayNames Prototype Object, https://tc39.es/ecma402/#sec-properties-of-intl-displaynames-prototype-object
DisplayNamesPrototype::DisplayNamesPrototype(GlobalObject& global_object)
    : Object(*global_object.object_prototype())
{
}

void DisplayNamesPrototype::initialize(GlobalObject& global_object)
{
    Object::initialize(global_object);

    auto& vm = this->vm();

    // 12.4.2 Intl.DisplayNames.prototype[ @@toStringTag ], https://tc39.es/ecma402/#sec-Intl.DisplayNames.prototype-@@tostringtag
    define_direct_property(*vm.well_known_symbol_to_string_tag(), js_string(vm, "Intl.DisplayNames"), Attribute::Configurable);

    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_native_function(vm.names.of, of, 1, attr);
}

// 12.4.3 Intl.DisplayNames.prototype.of ( code ), https://tc39.es/ecma402/#sec-Intl.DisplayNames.prototype.of
JS_DEFINE_NATIVE_FUNCTION(DisplayNamesPrototype::of)
{
    auto code = vm.argument(0);

    // 1. Let displayNames be this value.
    // 2. Perform ? RequireInternalSlot(displayNames, [[InitializedDisplayNames]]).
    auto* display_names = typed_this(global_object);
    if (!display_names)
        return {};

    // 3. Let code be ? ToString(code).
    auto code_string = code.to_string(global_object);
    if (vm.exception())
        return {};
    code = js_string(vm, move(code_string));

    // 4. Let code be ? CanonicalCodeForDisplayNames(displayNames.[[Type]], code).
    code = canonical_code_for_display_names(global_object, display_names->type(), code.as_string().string());
    if (vm.exception())
        return {};

    // 5. Let fields be displayNames.[[Fields]].
    // 6. If fields has a field [[<code>]], return fields.[[<code>]].
    Optional<StringView> result;

    switch (display_names->type()) {
    case DisplayNames::Type::Language:
        break;
    case DisplayNames::Type::Region:
        result = Unicode::get_locale_territory_mapping(display_names->locale(), code.as_string().string());
        break;
    case DisplayNames::Type::Script:
        break;
    case DisplayNames::Type::Currency:
        break;
    default:
        VERIFY_NOT_REACHED();
    }

    if (result.has_value())
        return js_string(vm, result.release_value());

    // 7. If displayNames.[[Fallback]] is "code", return code.
    if (display_names->fallback() == DisplayNames::Fallback::Code)
        return code;

    // 8. Return undefined.
    return js_undefined();
}

}
