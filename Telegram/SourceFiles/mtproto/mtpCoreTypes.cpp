/*
This file is part of Telegram Desktop,
an unofficial desktop messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014 John Preston, https://tdesktop.com
*/
#include "stdafx.h"
#include "mtpCoreTypes.h"

#if defined _DEBUG || defined _WITH_DEBUG

QString mtpWrapNumber(float64 number) {
	return QString::number(number);
}

void mtpTextSerializeCore(MTPStringLogger &to, const mtpPrime *&from, const mtpPrime *end, mtpTypeId cons, uint32 level, mtpPrime vcons) {
	QString add = QString(" ").repeated(level * 2);

	switch (mtpTypeId(cons)) {
	case mtpc_int: {
		MTPint value(from, end, cons);
		to.add(mtpWrapNumber(value.v)).add(" [INT]");
	} break;

	case mtpc_long: {
		MTPlong value(from, end, cons);
		to.add(mtpWrapNumber(value.v)).add(" [LONG]");
	} break;

	case mtpc_int128: {
		MTPint128 value(from, end, cons);
		to.add(mtpWrapNumber(value.h)).add(" * 2^64 + ").add(mtpWrapNumber(value.l)).add(" [INT128]");
	} break;

	case mtpc_int256: {
		MTPint256 value(from, end, cons);
		to.add(mtpWrapNumber(value.h.h)).add(" * 2^192 + ").add(mtpWrapNumber(value.h.l)).add(" * 2^128 + ").add(mtpWrapNumber(value.l.h)).add(" * 2 ^ 64 + ").add(mtpWrapNumber(value.l.l)).add(" [INT256]");
	} break;

	case mtpc_double: {
		MTPdouble value(from, end, cons);
		to.add(mtpWrapNumber(value.v)).add(" [DOUBLE]");
	} break;

	case mtpc_string: {
		MTPstring value(from, end, cons);
		QByteArray strUtf8(value.c_string().v.c_str(), value.c_string().v.length());
		QString str = QString::fromUtf8(strUtf8);
		if (str.toUtf8() == strUtf8) {
			to.add("\"").add(str.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n")).add("\" [STRING]");
		} else {
			to.add(mb(strUtf8.constData(), strUtf8.size()).str()).add(" [").add(mtpWrapNumber(strUtf8.size())).add(" BYTES]");
		}
	} break;

	case mtpc_boolTrue:
	case mtpc_boolFalse: {
		MTPbool value(from, end, cons);
		to.add(value.v ? "[TRUE]" : "[FALSE]");
	} break;

	case mtpc_vector: {
		if (from >= end) {
			throw Exception("from >= end in vector");
		}
		int32 cnt = *(from++);
		to.add("[ vector<0x").add(mtpWrapNumber(vcons, 16)).add(">");
		if (cnt) {
			to.add("\n").add(add);
			for (int32 i = 0; i < cnt; ++i) {
				to.add("  ");
				mtpTextSerializeType(to, from, end, vcons, level + 1);
				to.add(",\n").add(add);
			}
		} else {
			to.add(" ");
		}
		to.add("]");
	} break;

	case mtpc_error: {
		to.add("{ error");
		to.add("\n").add(add);
		to.add("  code: "); mtpTextSerializeType(to, from, end, mtpc_int, level + 1); to.add(",\n").add(add);
		to.add("  text: "); mtpTextSerializeType(to, from, end, mtpc_string, level + 1); to.add(",\n").add(add);
		to.add("}");
	} break;

	case mtpc_null: {
		to.add("{ null");
		to.add(" ");
		to.add("}");
	} break;

	case mtpc_rpc_result: {
		to.add("{ rpc_result");
		to.add("\n").add(add);
		to.add("  req_msg_id: "); mtpTextSerializeType(to, from, end, mtpc_long, level + 1); to.add(",\n").add(add);
		to.add("  result: "); mtpTextSerializeType(to, from, end, 0, level + 1); to.add(",\n").add(add);
		to.add("}");
	} break;

	case mtpc_msg_container: {
		to.add("{ msg_container");
		to.add("\n").add(add);
		to.add("  messages: "); mtpTextSerializeType(to, from, end, mtpc_vector, level + 1, mtpc_core_message); to.add(",\n").add(add);
		to.add("}");
	} break;

	case mtpc_core_message: {
		to.add("{ core_message");
		to.add("\n").add(add);
		to.add("  msg_id: "); mtpTextSerializeType(to, from, end, mtpc_long, level + 1); to.add(",\n").add(add);
		to.add("  seq_no: "); mtpTextSerializeType(to, from, end, mtpc_int, level + 1); to.add(",\n").add(add);
		to.add("  bytes: "); mtpTextSerializeType(to, from, end, mtpc_int, level + 1); to.add(",\n").add(add);
		to.add("  body: "); mtpTextSerializeType(to, from, end, 0, level + 1); to.add(",\n").add(add);
		to.add("}");
	} break;

	case mtpc_gzip_packed: {
		MTPstring packed(from, end); // read packed string as serialized mtp string type
		uint32 packedLen = packed.c_string().v.size(), unpackedChunk = packedLen;
		mtpBuffer result; // * 4 because of mtpPrime type
		result.resize(0);

		z_stream stream;
		stream.zalloc = 0;
		stream.zfree = 0;
		stream.opaque = 0;
		stream.avail_in = 0;
		stream.next_in = 0;
		int res = inflateInit2(&stream, 16 + MAX_WBITS);
		if (res != Z_OK) {
			throw Exception(QString("ungzip init, code: %1").arg(res));
		}
		stream.avail_in = packedLen;
		stream.next_in = (Bytef*)&packed._string().v[0];
		stream.avail_out = 0;
		while (!stream.avail_out) {
			result.resize(result.size() + unpackedChunk);
			stream.avail_out = unpackedChunk * sizeof(mtpPrime);
			stream.next_out = (Bytef*)&result[result.size() - unpackedChunk];
			int res = inflate(&stream, Z_NO_FLUSH);
			if (res != Z_OK && res != Z_STREAM_END) {
				inflateEnd(&stream);
				throw Exception(QString("ungzip unpack, code: %1").arg(res));
			}
		}
		if (stream.avail_out & 0x03) {
			uint32 badSize = result.size() * sizeof(mtpPrime) - stream.avail_out;
			throw Exception(QString("ungzip bad length, size: %1").arg(badSize));
		}
		result.resize(result.size() - (stream.avail_out >> 2));
		inflateEnd(&stream);

		if (!result.size()) {
			throw Exception("ungzip void data");
		}
		const mtpPrime *newFrom = result.constData(), *newEnd = result.constData() + result.size();
		to.add("[GZIPPED] "); mtpTextSerializeType(to, newFrom, newEnd, 0, level);
	} break;

	default: {
		for (uint32 i = 1; i < mtpLayerMax; ++i) {
			if (cons == mtpLayers[i]) {
				to.add("[LAYER").add(mtpWrapNumber(i + 1)).add("] "); mtpTextSerializeType(to, from, end, 0, level);
				return;
			}
		}
		throw Exception(QString("unknown cons 0x%1").arg(cons, 0, 16));
	} break;
	}
}

#endif
