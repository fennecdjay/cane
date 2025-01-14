#ifndef CANE_COMPILE_HPP
#define CANE_COMPILE_HPP

namespace cane {

constexpr decltype(auto) is(Symbols kind) {
	return [=] (Token other) { return kind == other.kind; };
}

[[nodiscard]] inline double   literal_expr  (Context&, Lexer&, View, size_t);
[[nodiscard]] inline Sequence sequence_expr (Context&, Lexer&, View, size_t);

constexpr bool is_literal(Token x) {
	return cmp_any(x.kind,
		Symbols::INT);
}

constexpr bool is_step(Token x) {
	return cmp_any(x.kind,
		Symbols::SKIP,
		Symbols::BEAT);
}

constexpr bool is_literal_prefix(Token x) {
	return cmp_any(x.kind,
		Symbols::LEN_OF,
		Symbols::BEAT_OF,
		Symbols::SKIP_OF);
}

constexpr bool is_literal_infix(Token x) {
	return cmp_any(x.kind,
		Symbols::ADD,
		Symbols::SUB,
		Symbols::MUL,
		Symbols::DIV);
}

constexpr bool is_sequence_prefix(Token x) {
	return cmp_any(x.kind,
		Symbols::INVERT,
		Symbols::REV);
}

constexpr bool is_sequence_postfix(Token x) {
	return cmp_any(x.kind,
		Symbols::CAR,
		Symbols::CDR,
		Symbols::DBG);
}

constexpr bool is_sequence_infix(Token x) {
	return cmp_any(x.kind,
		Symbols::OR,
		Symbols::AND,
		Symbols::XOR,
		Symbols::CAT,
		Symbols::ROTL,
		Symbols::ROTR,
		Symbols::REP,
		Symbols::BPM,
		Symbols::MAP,
		Symbols::CHAIN);
}

constexpr bool is_sequence_primary(Token x) {
	return cmp_any(x.kind,
		Symbols::IDENT,
		Symbols::LPAREN,
		Symbols::SEP) or
		is_literal(x) or
		is_step(x);
}

constexpr bool is_literal_primary(Token x) {
	return cmp_any(x.kind,
		Symbols::IDENT,
		Symbols::LPAREN,
		Symbols::GLOBAL_BPM,
		Symbols::GLOBAL_NOTE) or
		is_literal(x);
}

constexpr bool is_meta(Token x) {
	return cmp_any(x.kind,
		Symbols::GLOBAL_NOTE,
		Symbols::GLOBAL_BPM);
}

enum class OpFix {
	LIT_PREFIX,
	LIT_INFIX,
	SEQ_PREFIX,
	SEQ_INFIX,
	SEQ_POSTFIX,
};

inline std::pair<size_t, size_t> binding_power(Context& ctx, Lexer& lx, Token tok, OpFix fix) {
	auto [view, kind] = tok;

	enum { LEFT = 1, RIGHT = 0, };

	enum {
		DBG,
		CHAIN = DBG,
		MAP   = DBG,

		CAR,
		CDR = CAR,

		CAT,
		OR   = CAT,
		AND  = CAT,
		XOR  = CAT,
		ROTL = CAT,
		ROTR = CAT,
		REP  = CAT,
		BPM  = CAT,

		REV,
		INVERT = REV,

		ADD,
		SUB = ADD,

		MUL,
		DIV = MUL,

		LEN_OF,
		BEAT_OF = LEN_OF,
		SKIP_OF = LEN_OF,
	};

	switch (fix) {
		case OpFix::LIT_PREFIX: switch (kind) {
			case Symbols::LEN_OF:  return { 0u, LEN_OF  + RIGHT };
			case Symbols::BEAT_OF: return { 0u, BEAT_OF + RIGHT };
			case Symbols::SKIP_OF: return { 0u, SKIP_OF + RIGHT };
			default: break;
		} break;

		case OpFix::LIT_INFIX: switch (kind) {
  			case Symbols::ADD: return { ADD, ADD + LEFT };
			case Symbols::SUB: return { SUB, SUB + LEFT };
			case Symbols::MUL: return { MUL, MUL + LEFT };
			case Symbols::DIV: return { DIV, DIV + LEFT };
			default: break;
		} break;

		case OpFix::SEQ_PREFIX: switch (kind) {
			case Symbols::REV:    return { 0u, REV    + RIGHT };
			case Symbols::INVERT: return { 0u, INVERT + RIGHT };
			default: break;
		} break;

		case OpFix::SEQ_INFIX: switch (kind) {
			case Symbols::MAP:   return { MAP,   MAP   + LEFT };
			case Symbols::CHAIN: return { CHAIN, CHAIN + LEFT };
			case Symbols::CAT:   return { CAT,   CAT   + LEFT };
			case Symbols::OR:    return { OR,    OR    + LEFT };
			case Symbols::AND:   return { AND,   AND   + LEFT };
			case Symbols::XOR:   return { XOR,   XOR   + LEFT };
			case Symbols::REP:   return { REP,   REP   + LEFT };
			case Symbols::ROTL:  return { ROTL,  ROTL  + LEFT };
			case Symbols::ROTR:  return { ROTR,  ROTR  + LEFT };
			case Symbols::BPM:   return { BPM,   BPM   + LEFT };
			default: break;
		} break;

		case OpFix::SEQ_POSTFIX: switch(kind) {
			case Symbols::DBG: return { DBG, DBG + LEFT };
			case Symbols::CAR: return { CAR, CAR + LEFT };
			case Symbols::CDR: return { CDR, CDR + LEFT };
			default: break;
		} break;
	}

	lx.error(ctx, Phases::INTERNAL, view, STR_UNREACHABLE, sym2str(kind));
}

inline double literal(Context& ctx, Lexer& lx, View lit_v) {
	CANE_LOG(LogLevel::INF);

	lx.expect(ctx, is_literal, lx.peek.view, STR_LITERAL);
	auto [view, kind] = lx.next();

	return b10_decode(view);
}

inline Sequence sequence(Context& ctx, Lexer& lx, View expr_v, Sequence seq) {
	CANE_LOG(LogLevel::INF);

	lx.expect(ctx, is_step, lx.peek.view, STR_STEP);

	while (is_step(lx.peek))
		seq.emplace_back(sym2step(lx.next().kind));

	return seq;
}

inline Sequence euclide(Context& ctx, Lexer& lx, View expr_v, Sequence seq) {
	CANE_LOG(LogLevel::INF);

	uint64_t steps = 0;
	uint64_t beats = 0;

	if (lx.peek.kind == Symbols::SEP) {
		lx.next();  // skip `:`
		beats = literal_expr(ctx, lx, lx.peek.view, 0);
	}

	else
		beats = literal(ctx, lx, lx.peek.view);

	lx.expect(ctx, is(Symbols::SEP), lx.peek.view, STR_EXPECT, sym2str(Symbols::SEP));
	lx.next();  // skip `:`

	steps = literal_expr(ctx, lx, lx.peek.view, 0);

	if (beats > steps)
		lx.error(ctx, Phases::SEMANTIC, encompass(expr_v, lx.prev.view), STR_LESSER_EQ, steps);

	for (size_t i = 0; i != static_cast<size_t>(steps); ++i)
		seq.emplace_back(((i * beats) % steps) < static_cast<size_t>(beats));

	if (seq.empty())
		lx.error(ctx, Phases::SEMANTIC, encompass(expr_v, lx.prev.view), STR_EMPTY);

	return seq;
}

inline double literal_const(Context& ctx, Lexer& lx, View lit_v) {
	CANE_LOG(LogLevel::INF);

	lx.expect(ctx, is(Symbols::IDENT), lx.peek.view, STR_IDENT);
	auto [view, kind] = lx.next();

	if (auto it = ctx.constants.find(view); it != ctx.constants.end())
		return it->second;

	lx.error(ctx, Phases::SEMANTIC, view, STR_UNDEFINED, view);
}

inline Sequence sequence_const(Context& ctx, Lexer& lx, View expr_v) {
	CANE_LOG(LogLevel::INF);

	lx.expect(ctx, is(Symbols::IDENT), lx.peek.view, STR_IDENT);
	auto [view, kind] = lx.next();

	if (auto it = ctx.chains.find(view); it != ctx.chains.end())
		return it->second;

	lx.error(ctx, Phases::SEMANTIC, view, STR_UNDEFINED, view);
}

inline double literal_primary(Context& ctx, Lexer& lx, View lit_v, double lit, size_t bp) {
	CANE_LOG(LogLevel::INF);

	Token tok = lx.peek;
	CANE_LOG(LogLevel::INF, sym2str(tok.kind));

	switch (tok.kind) {
		case Symbols::INT: {
			lit = literal(ctx, lx, lx.peek.view);
		} break;

		case Symbols::IDENT: {
			lit = literal_const(ctx, lx, lx.peek.view);
		} break;

		case Symbols::GLOBAL_BPM: {
			lx.next();  // skip `bpm`
			lit = ctx.global_bpm;
		} break;

		case Symbols::GLOBAL_NOTE: {
			lx.next();  // skip `note`
			lit = ctx.global_note;
		} break;

		case Symbols::LPAREN: {
			lx.next();  // skip `(`

			lit = literal_expr(ctx, lx, lit_v, 0);  // Reset binding power.

			lx.expect(ctx, is(Symbols::RPAREN), lx.peek.view, STR_EXPECT, sym2str(Symbols::RPAREN));
			lx.next();  // skip `)`
		} break;

		default: { lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_LIT_PRIMARY); } break;
	}

	return lit;
}

inline double literal_prefix(Context& ctx, Lexer& lx, View lit_v, double lit, size_t bp) {
	CANE_LOG(LogLevel::INF);

	Token tok = lx.next();
	CANE_LOG(LogLevel::INF, sym2str(tok.kind));

	switch (tok.kind) {
		case Symbols::LEN_OF:  { lit = sequence_len   (sequence_expr(ctx, lx, tok.view, bp)); } break;
		case Symbols::BEAT_OF: { lit = sequence_beats (sequence_expr(ctx, lx, tok.view, bp)); } break;
		case Symbols::SKIP_OF: { lit = sequence_skips (sequence_expr(ctx, lx, tok.view, bp)); } break;

		default: { lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_LIT_OPERATOR); } break;
	}

	return lit;
}

inline double literal_infix(Context& ctx, Lexer& lx, View lit_v, double lit, size_t bp) {
	CANE_LOG(LogLevel::INF);

	Token tok = lx.next();
	CANE_LOG(LogLevel::INF, sym2str(tok.kind));

	switch (tok.kind) {
		case Symbols::ADD: { lit = lit + literal_expr(ctx, lx, lit_v, bp); } break;
		case Symbols::SUB: { lit = lit - literal_expr(ctx, lx, lit_v, bp); } break;
		case Symbols::MUL: { lit = lit * literal_expr(ctx, lx, lit_v, bp); } break;
		case Symbols::DIV: { lit = lit / literal_expr(ctx, lx, lit_v, bp); } break;

		default: { lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_LIT_OPERATOR); } break;
	}

	return lit;
}

inline double literal_expr(Context& ctx, Lexer& lx, View lit_v, size_t bp) {
	CANE_LOG(LogLevel::WRN);

	double lit = 0;
	Token tok = lx.peek;

	if (is_literal_prefix(tok)) {
		auto [lbp, rbp] = binding_power(ctx, lx, tok, OpFix::LIT_PREFIX);
		lit = literal_prefix(ctx, lx, lit_v, lit, rbp);
	}

	else if (is_literal_primary(tok))
		lit = literal_primary(ctx, lx, lit_v, lit, 0);

	else
		lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_LIT_PRIMARY);

	tok = lx.peek;

	while (is_literal_infix(tok)) {
		CANE_LOG(LogLevel::INF, sym2str(tok.kind));
		auto [lbp, rbp] = binding_power(ctx, lx, tok, OpFix::LIT_INFIX);

		if (lbp < bp)
			break;

		lit = literal_infix(ctx, lx, lit_v, lit, rbp);
		tok = lx.peek;
	}

	return lit;
}

inline uint8_t channel(Context& ctx, Lexer& lx) {
	CANE_LOG(LogLevel::INF);

	uint8_t chan = CHANNEL_MIN;
	Token tok = lx.peek;

	// Sink can be either a literal number or an alias defined previously.
	if (is_literal(tok))
		chan = literal(ctx, lx, lx.peek.view);

	else if (lx.peek.kind == Symbols::IDENT) {
		lx.next();  // skip identifier

		auto it = ctx.channels.find(tok.view);

		if (it == ctx.channels.end())
			lx.error(ctx, Phases::SEMANTIC, tok.view, STR_UNDEFINED, tok.view);

		chan = it->second;
	}

	else
		lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_IDENT_LITERAL);

	if (chan > CHANNEL_MAX or chan < CHANNEL_MIN)
		lx.error(ctx, Phases::SEMANTIC, tok.view, STR_BETWEEN, CHANNEL_MIN, CHANNEL_MAX);

	return chan - 1;
}

inline Sequence sequence_primary(Context& ctx, Lexer& lx, View expr_v, Sequence seq, size_t bp) {
	CANE_LOG(LogLevel::INF);

	Token tok = lx.peek;
	CANE_LOG(LogLevel::INF, sym2str(tok.kind));

	switch (tok.kind) {
		case Symbols::INT:
		case Symbols::SEP: {
			seq = euclide(ctx, lx, lx.peek.view, std::move(seq));
		} break;

		case Symbols::SKIP:
		case Symbols::BEAT: {
			seq = sequence(ctx, lx, lx.peek.view, std::move(seq));
		} break;

		case Symbols::IDENT: {
			seq = sequence_const(ctx, lx, lx.peek.view);
		} break;

		case Symbols::LPAREN: {
			lx.next();  // skip `(`

			seq = sequence_expr(ctx, lx, expr_v, 0);  // Reset binding power.

			lx.expect(ctx, is(Symbols::RPAREN), lx.peek.view, STR_EXPECT, sym2str(Symbols::RPAREN));
			lx.next();  // skip `)`
		} break;

		default: { lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_SEQ_PRIMARY); } break;
	}

	return seq;
}

inline Sequence sequence_prefix(Context& ctx, Lexer& lx, View expr_v, Sequence seq, size_t bp) {
	CANE_LOG(LogLevel::INF);

	Token tok = lx.next();
	CANE_LOG(LogLevel::INF, sym2str(tok.kind));

	switch (tok.kind) {
		case Symbols::REV:    { seq = sequence_reverse (sequence_expr(ctx, lx, expr_v, bp)); } break;
		case Symbols::INVERT: { seq = sequence_invert  (sequence_expr(ctx, lx, expr_v, bp)); } break;

		default: { lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_SEQ_OPERATOR); } break;
	}

	return seq;
}

inline Sequence sequence_infix(Context& ctx, Lexer& lx, View expr_v, Sequence seq, size_t bp) {
	CANE_LOG(LogLevel::INF);

	Token tok = lx.next();  // skip operator.
	CANE_LOG(LogLevel::INF, sym2str(tok.kind));

	switch (tok.kind) {
		case Symbols::CAT: { seq = sequence_cat (std::move(seq), sequence_expr(ctx, lx, expr_v, bp)); } break;
		case Symbols::OR:  { seq = sequence_or  (std::move(seq), sequence_expr(ctx, lx, expr_v, bp)); } break;
		case Symbols::AND: { seq = sequence_and (std::move(seq), sequence_expr(ctx, lx, expr_v, bp)); } break;
		case Symbols::XOR: { seq = sequence_xor (std::move(seq), sequence_expr(ctx, lx, expr_v, bp)); } break;

		case Symbols::ROTL: { seq = sequence_rotl (std::move(seq), literal_expr(ctx, lx, tok.view, 0)); } break;
		case Symbols::ROTR: { seq = sequence_rotr (std::move(seq), literal_expr(ctx, lx, tok.view, 0)); } break;

		case Symbols::REP: {
			View before_v = lx.peek.view;
			uint64_t reps = literal_expr(ctx, lx, before_v, 0);

			// We don't want to shrink the sequence, it can only grow.
			if (reps == 0)
				lx.error(ctx, Phases::SEMANTIC, encompass(before_v, lx.prev.view), STR_GREATER, 0);

			seq = sequence_repeat(std::move(seq), reps);
		} break;

		case Symbols::BPM: {
			View before_v = lx.peek.view;
			uint64_t bpm = literal_expr(ctx, lx, before_v, 0);
			seq.bpm = bpm;
		} break;

		case Symbols::MAP: {
			lx.expect(ctx, is_literal_primary, lx.peek.view, STR_LIT_EXPR);

			std::vector<uint64_t> notes;
			while (is_literal_primary(lx.peek)) {
				uint64_t note = literal_expr(ctx, lx, lx.peek.view, 0);
				notes.emplace_back(note);
			}

			size_t index = 0;
			for (auto& [note, kind]: seq) {
				note = notes[index];
				index = (index + 1) % notes.size();
			}
		} break;

		case Symbols::CHAIN: {
			lx.expect(ctx, is(Symbols::IDENT), lx.peek.view, STR_IDENT);
			auto [view, kind] = lx.next();

			// Assign or warn if re-assigned.
			if (auto [it, succ] = ctx.symbols.emplace(view); not succ)
				lx.error(ctx, Phases::SEMANTIC, view, STR_CONFLICT, view);

			if (auto [it, succ] = ctx.chains.try_emplace(view, seq); not succ)
	    		lx.error(ctx, Phases::SEMANTIC, view, STR_REDEFINED, view);
		} break;

		default: { lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_SEQ_OPERATOR); } break;
	}

	return seq;
}

inline Sequence sequence_postfix(Context& ctx, Lexer& lx, View expr_v, Sequence seq, size_t bp) {
	CANE_LOG(LogLevel::INF);

	Token tok = lx.next();  // skip operator.
	CANE_LOG(LogLevel::INF, sym2str(tok.kind));

	switch (tok.kind) {
		case Symbols::CAR: { seq = sequence_car(std::move(seq)); } break;
		case Symbols::CDR: { seq = sequence_cdr(std::move(seq)); } break;

		case Symbols::DBG: {
			auto mini = sequence_minify(seq);
			size_t count = seq.size() / mini.size();

			lx.notice(ctx, Phases::SEMANTIC, encompass(expr_v, tok.view), STR_DEBUG, mini, count, seq.size());
		} break;

		default: { lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_SEQ_OPERATOR); } break;
	}

	return seq;
}

inline Sequence sequence_expr(Context& ctx, Lexer& lx, View expr_v, size_t bp) {
	CANE_LOG(LogLevel::WRN);

	Sequence seq {};
	seq.bpm = ctx.global_bpm;

	Token tok = lx.peek;

	if (is_sequence_prefix(tok)) {
		auto [lbp, rbp] = binding_power(ctx, lx, tok, OpFix::SEQ_PREFIX);
		seq = sequence_prefix(ctx, lx, expr_v, std::move(seq), rbp);
	}

	else if (is_sequence_primary(tok))
		seq = sequence_primary(ctx, lx, expr_v, std::move(seq), 0);

	else
		lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_SEQ_PRIMARY);

	tok = lx.peek;

	while (
		is_sequence_infix(tok) or
		is_sequence_postfix(tok)
	) {
		CANE_LOG(LogLevel::INF, sym2str(tok.kind));

		if (is_sequence_postfix(tok)) {
			auto [lbp, rbp] = binding_power(ctx, lx, tok, OpFix::SEQ_POSTFIX);
			if (lbp < bp)
				break;

			seq = sequence_postfix(ctx, lx, expr_v, std::move(seq), 0);
		}

		else if (is_sequence_infix(tok)) {
			auto [lbp, rbp] = binding_power(ctx, lx, tok, OpFix::SEQ_INFIX);
			if (lbp < bp)
				break;

			seq = sequence_infix(ctx, lx, expr_v, std::move(seq), rbp);
		}

		else
			lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_SEQ_OPERATOR);

		tok = lx.peek;
	}

	return seq;
}

inline Timeline sequence_compile(Sequence seq, uint8_t chan, Unit time) {
	CANE_LOG(LogLevel::INF);

	Timeline tl {};

	auto per = ONE_MIN / seq.bpm;

	auto ON = midi2int(Midi::NOTE_ON) | chan;
	auto OFF = midi2int(Midi::NOTE_OFF) | chan;

	for (auto& [note, kind]: seq) {
		if (kind == BEAT) {
			tl.emplace_back(time, ON, note, VELOCITY_DEFAULT);
			tl.emplace_back(time + per, OFF, note, VELOCITY_DEFAULT);
		}

		time += per;
	}

	tl.duration = time;

	return tl;
}

inline Timeline send(Context& ctx, Lexer& lx, View stat_v, Unit time) {
	CANE_LOG(LogLevel::INF);

	lx.expect(ctx, is(Symbols::SEND), lx.peek.view, STR_EXPECT, sym2str(Symbols::SEND));
	lx.next();  // skip `send`

	uint8_t chan = channel(ctx, lx);
	Sequence seq = sequence_expr(ctx, lx, lx.peek.view, 0);

	Timeline tl = sequence_compile(std::move(seq), chan, time);

	return tl;
}

inline void statement(Context& ctx, Lexer& lx, View stat_v) {
	CANE_LOG(LogLevel::WRN);

	Token tok = lx.peek;

	if (tok.kind == Symbols::ALIAS) {
		CANE_LOG(LogLevel::INF, sym2str(Symbols::ALIAS));
		lx.next();  // skip `alias`

		lx.expect(ctx, is(Symbols::IDENT), lx.peek.view, STR_IDENT);
		auto [view, kind] = lx.next();  // get identifier

		uint8_t chan = literal(ctx, lx, lx.peek.view);

		if (chan > CHANNEL_MAX or chan < CHANNEL_MIN)
			lx.error(ctx, Phases::SEMANTIC, lx.prev.view, STR_BETWEEN, CHANNEL_MIN, CHANNEL_MAX);

		// Assign or warn if re-assigned.
		if (auto [it, succ] = ctx.symbols.emplace(view); not succ)
			lx.error(ctx, Phases::SEMANTIC, view, STR_CONFLICT, view);

		if (auto [it, succ] = ctx.channels.try_emplace(view, chan); not succ)
			lx.error(ctx, Phases::SEMANTIC, view, STR_REDEFINED, view);
	}

	else if (tok.kind == Symbols::LET) {
		CANE_LOG(LogLevel::INF, sym2str(Symbols::LET));
		lx.next();  // skip `let`

		lx.expect(ctx, is(Symbols::IDENT), lx.peek.view, STR_IDENT);
		auto [view, kind] = lx.next();  // get identifier

		double lit = literal_expr(ctx, lx, lx.peek.view, 0);

		// Assign or warn if re-assigned.
		if (auto [it, succ] = ctx.symbols.emplace(view); not succ)
			lx.error(ctx, Phases::SEMANTIC, view, STR_CONFLICT, view);

		if (auto [it, succ] = ctx.constants.try_emplace(view, lit); not succ)
			lx.error(ctx, Phases::SEMANTIC, view, STR_REDEFINED, view);
	}

	else if (is_sequence_primary(tok) or is_sequence_prefix(tok)) {
		Sequence seq = sequence_expr(ctx, lx, lx.peek.view, 0);
	}

	else if (tok.kind == Symbols::SEND) {
		Unit orig = ctx.time;
		Timeline tl = send(ctx, lx, lx.peek.view, ctx.time);

		ctx.time = std::max(tl.duration, ctx.time);
		ctx.tl.duration = std::max(tl.duration, ctx.tl.duration);

		ctx.tl.insert(ctx.tl.end(), tl.begin(), tl.end());

		while (lx.peek.kind == Symbols::WITH) {
			lx.next();  // skip `$`

			Timeline tl = send(ctx, lx, lx.peek.view, orig);

			ctx.time = std::max(tl.duration, ctx.time);
			ctx.tl.duration = std::max(tl.duration, ctx.tl.duration);

			ctx.tl.insert(ctx.tl.end(), tl.begin(), tl.end());
		}
	}

	else
		lx.error(ctx, Phases::SYNTACTIC, tok.view, STR_STATEMENT);
}

inline Timeline compile(
	View src,
	Handler&& error_handler,
	Handler&& warning_handler,
	Handler&& notice_handler
) {
	CANE_LOG(LogLevel::WRN);

	Context ctx { std::move(error_handler), std::move(warning_handler), std::move(notice_handler) };
	Lexer lx { src, ctx };

	lx.next(); // important

	if (not cane::validate(src))
		lx.error(ctx, cane::Phases::ENCODING, src, cane::STR_ENCODING);

	// Compile
	enum {
		META_NONE,
		META_NOTE = 0b01,
		META_BPM  = 0b10,
	};

	uint8_t flags = META_NONE;

	while (is_meta(lx.peek)) {
		auto [view, kind] = lx.next();

		switch (kind) {
			case Symbols::GLOBAL_BPM: {
				CANE_LOG(LogLevel::INF, sym2str(Symbols::GLOBAL_BPM));
				uint64_t bpm = literal_expr(ctx, lx, lx.peek.view, 0);
				ctx.global_bpm = bpm;
				flags |= META_BPM;
			} break;

			case Symbols::GLOBAL_NOTE: {
				CANE_LOG(LogLevel::INF, sym2str(Symbols::GLOBAL_NOTE));
				uint64_t note = literal_expr(ctx, lx, lx.peek.view, 0);
				ctx.global_note = note;
				flags |= META_NOTE;
			} break;

			default: { lx.error(ctx, Phases::SYNTACTIC, view, STR_META); } break;
		}
	}

	if ((flags & META_BPM) != META_BPM)
		lx.error(ctx, Phases::SEMANTIC, lx.peek.view, STR_NO_BPM);

	if ((flags & META_NOTE) != META_NOTE)
		lx.error(ctx, Phases::SEMANTIC, lx.peek.view, STR_NO_NOTE);

	while (lx.peek.kind != Symbols::TERMINATOR)
		statement(ctx, lx, lx.peek.view);

	Timeline tl = std::move(ctx.tl);

	if (tl.empty())
		return tl;

	// Active sensing
	Unit t = Unit::zero();
	while (t < tl.duration) {
		tl.emplace_back(t, midi2int(Midi::ACTIVE_SENSE), 0, 0);
		t += ACTIVE_SENSING_INTERVAL;
	}

	// MIDI clock pulse
	// We fire off a MIDI tick 24 times
	// for every quarter note
	Unit clock_freq = std::chrono::duration_cast<cane::Unit>(std::chrono::minutes { 1 }) / (ctx.global_bpm * 24);
	t = Unit::zero();
	while (t < tl.duration) {
		tl.emplace_back(t, midi2int(Midi::TIMING_CLOCK), 0, 0);
		t += clock_freq;
	}

	// Sort sequence by timestamps
	std::stable_sort(tl.begin(), tl.end(), [] (auto& a, auto& b) {
		return a.time < b.time;
	});

	// Start/Stop
	tl.emplace(tl.begin(), Unit::zero(), midi2int(Midi::START), 0, 0);
	tl.emplace(tl.end(), tl.duration, midi2int(Midi::STOP), 0, 0);

	// Reset state of MIDI devices
	for (size_t i = CHANNEL_MIN; i != CHANNEL_MAX; ++i) {
		tl.emplace(tl.begin(), Unit::zero(), midi2int(Midi::CHANNEL_MODE), ALL_SOUND_OFF, 0);
		tl.emplace(tl.begin(), Unit::zero(), midi2int(Midi::CHANNEL_MODE), ALL_NOTES_OFF, 0);
		tl.emplace(tl.begin(), Unit::zero(), midi2int(Midi::CHANNEL_MODE), ALL_RESET_CC, 0);
	}

	return tl;
}

}

#endif
