#ifndef PARSER_HH
#define PARSER_HH

/* 
 * Code for generating rules from a vector of tokens, i.e, for
 * performing the parsing of Stu syntax beyond tokenization.  This is a
 * recursive descent parser written by hand.
 */ 

#include <set>

#include "rule.hh"
#include "token.hh"
#include "dep.hh"

/*
 * Stu has only prefix and circumfix operators, and therefore its syntax
 * is trivial, i.e., there are no ambiguities, and no need to consider
 * precendence levels or associativity.
 *
 * A Yacc-like description of Stu syntax is given in the manpage. 
 *
 * In principle, the following gives operator precedences.  Higher in
 * the list means higher precedence.  This list is to be understood as
 * specifying in what order operators can be nested, not to disambguate
 * expressions.
 *
 * @...	     (prefix) Transient dependency; argument can only contain name
 * ---------------
 * <...	     (prefix) Input redirection; argument must not contain '()',
 *           '[]', '$[]' or '@' 
 * ---------------
 * !...	     (prefix) Persistent dependency; argument must not contain '$[]'
 * ?...	     (prefix) Optional dependency; argument must not contain '$[]'
 * &...      (prefix) Trivial dependency
 * ---------------
 * [...]     (circumfix) Dynamic dependency; must not contain '$[]' or '@'
 * (...)     (circumfix) Capture
 * $[...]    (circumfix) Variable inclusion; argument must not contain
 *           -o, '[]', '()', '*' or '@' 
 */

/* 
 * This code does not check that imcompatible constructs (like -p and -o
 * or -p and '$[') are used together.  Instead, this is checked within
 * Execution and not here, because these can also be combined from
 * different sources, e.g., a file and a dynamic dependency.
 */ 

class Parser
/* 
 * An object of this type represents a location within a token list.
 */ 
{
public:

	/*
	 * Methods for building the syntax tree:  Each has a name
	 * corresponding to the symbol given by the Yacc syntax in the
	 * manpage.  The argument RET (if it is used) is where the
	 * result is written. 
	 * If the return value is BOOL, it denotes whether something was
	 * read or not.  On syntax errors, ERROR_LOGICAL is thrown. 
	 */

	/* In some of the following functions, write the input filename into
	 * PLACE_NAME_INPUT.  If PLACE_NAME_INPUT is already non-empty,
	 * throw an error if a second input filename is specified.
	 * PLACE_INPUT is the place of the '<' input redirection
	 * operator.  */ 

	static void get_rule_list(vector <shared_ptr <const Rule> > &rules,
				  vector <shared_ptr <Token> > &tokens,
				  const Place &place_end);

	static void get_expression_list(vector <shared_ptr <const Dep> > &deps,
					vector <shared_ptr <Token> > &tokens,
					const Place &place_end,
					Place_Name &input,
					Place &place_input);
	/* DEPENDENCIES is filled.  DEPENDENCIES is empty when called.
	 * TARGET is used for error messages.  Empty when in a dynamic
	 * dependency.  */

	static shared_ptr <const Dep> get_target_dep(string text, const Place &place); 
	/* Parse a dependency as given on the command line outside of
	 * options.  This supports only the characters '@' and '[]', as
	 * well as names.  TEXT must not be "".  */

private:

	vector <shared_ptr <Token> > &tokens;
	vector <shared_ptr <Token> > ::iterator &iter;
	const Place place_end; 

	Parser(vector <shared_ptr <Token> > &tokens_,
	       vector <shared_ptr <Token> > ::iterator &iter_,
	       const Place &place_end_)
		:  tokens(tokens_),
		   iter(iter_),
		   place_end(place_end_)
	{ }
	
	void parse_rule_list(vector <shared_ptr <const Rule> > &ret);
	/* The returned rules may not be unique -- this is checked later */ 

	bool parse_expression_list(vector <shared_ptr <const Dep> > &ret, 
				   Place_Name &place_name_input,
				   Place &place_input,
				   const vector <shared_ptr <const Place_Param_Target> > &targets);
	/* RET is filled.  RET is empty when called. */

	shared_ptr <const Rule> parse_rule(); 
	/* Return null when nothing was parsed */ 

	bool parse_expression(shared_ptr <const Dep> &ret,
			      Place_Name &place_name_input,
			      Place &place_input,
			      const vector <shared_ptr <const Place_Param_Target> > &targets);
	/* Parse an expression.  Write the parsed expression into RET.
	 * RET must be empty when called.  Return whether an expression
	 * was parsed.  TARGETS is passed to construct error
	 * messages.  */

	shared_ptr <const Dep> parse_variable_dep
	(Place_Name &place_name_input,
	 Place &place_input,
	 const vector <shared_ptr <const Place_Param_Target> > &targets);
	/* A variable dependency */ 

	shared_ptr <const Dep> parse_redirect_dep
	(Place_Name &place_name_input,
	 Place &place_input,
	 const vector <shared_ptr <const Place_Param_Target> > &targets);

	/* If the next token is of type T, return it, otherwise return
	 * null.  Also return null when at the end of the token list.  */
	template <typename T>
	shared_ptr <T> is() const {
		if (iter == tokens.end())
			return nullptr;
		else 
			return dynamic_pointer_cast <T> (*iter); 
	}

	/* Whether the next token is the given operator */ 
	bool is_operator(char op) const {
		return is <Operator> () && is <Operator> ()->op == op;
	}

	/* Whether the next token is the given flag token */ 
	bool is_flag(char flag) const {
		return is <Flag_Token> () && is <Flag_Token> ()->flag == flag;
	}

	bool next_concatenates() const;
	/* Whether there is a next token which concatenates to the
	 * current token.  The current token is assumed to be a
	 * candidate for concatenation.  */
	
	static void print_separation_message(shared_ptr <const Token> token); 

	static void append_copy(      Name &to,
				const Name &from);
	/* If TO ends in '/', append to it the part of FROM that
	 * comes after the last slash, or the full target if it contains
	 * no slashes.  Parameters are not considered for containing
	 * slashes */
};

void Parser::parse_rule_list(vector <shared_ptr <const Rule> > &ret)
{
	assert(ret.size() == 0); 
	
	while (iter != tokens.end()) {

#ifndef NDEBUG
		const auto iter_begin= iter; 
#endif /* ! NDEBUG */ 

		shared_ptr <const Rule> rule= parse_rule(); 

		if (rule == nullptr) {
			assert(iter == iter_begin); 
			break;
		}

		ret.push_back(rule); 
	}
}

shared_ptr <const Rule> Parser::parse_rule()
{
	const auto iter_begin= iter;
	/* Used to check that when this function fails (i.e., returns
	 * null), is has not read any tokens. */ 

	Place place_output; 
	/* T_EMPTY when output is not redirected */

	int redirect_index= -1; 
	/* Index of the target that has the output, or -1 */ 

	vector <shared_ptr <const Place_Param_Target> > place_param_targets; 

	while (iter != tokens.end()) {

		Place place_output_new; 
		/* Remains EMPTY when '>' is not present */ 
		
		if (is_operator('>')) {
			place_output_new= (*iter)->get_place();
			++iter; 
		}

		Flags flags_type= 0;
		/* F_TARGET_TRANSIENT is set when '@' is found */ 

		Place place_target;
		if (iter != tokens.end()) 
			place_target= (*iter)->get_place_start();

		if (is_operator('@')) {
			Place place_at= (*iter)->get_place();
			++iter;

			if (iter == tokens.end()) {
				place_end << "expected the name of transient target";
				place_at << fmt("after %s", char_format_word('@'));
				throw ERROR_LOGICAL;
			}
			if (! is <Name_Token> ()) {
				(*iter)->get_place_start() 
					<< fmt("expected the name of transient target, not %s",
					       (*iter)->format_start_word()); 
				place_at << fmt("after %s", char_format_word('@'));
				throw ERROR_LOGICAL;
			}

			if (! place_output_new.empty()) {
				Target target(F_TARGET_TRANSIENT, is <Name_Token> ()->raw()); 
				place_at << 
					fmt("transient target %s is invalid",
					    target.format_word()); 
				place_output_new << fmt("after output redirection using %s", 
							char_format_word('>'));
				throw ERROR_LOGICAL;
			}

			flags_type= F_TARGET_TRANSIENT; 
		}
		
		if (! is <Name_Token> ()) {
			if (! place_output_new.empty()) {
				if (iter == tokens.end()) {
					place_end << "expected a filename";
					place_output_new << 
						fmt("after output redirection using %s",
						    char_format_word('>'));
					throw ERROR_LOGICAL;
				}
				else {
					(*iter)->get_place_start() << 
						fmt("expected a filename, not %s",
						    (*iter)->format_start_word());
					place_output_new << 
						fmt("after output redirection using %s", 
						    char_format_word('>'));
					throw ERROR_LOGICAL;
				}
			}
			break;
		}

		/* Target */ 
		shared_ptr <Name_Token> target_name= is <Name_Token> ();
		++iter;

		if (! place_output_new.empty()) {
			if (! place_output.empty()) {
				place_output_new <<
					fmt("there must not be a second output redirection %s",
					    prefix_format_word(target_name->raw(), ">")); 
				assert(place_param_targets[redirect_index]
				       ->place_name.get_n() == 0);
				assert((place_param_targets[redirect_index]->flags & F_TARGET_TRANSIENT) == 0); 
				place_output <<
					fmt("shadowing previous output redirection %s",
					    prefix_format_word
					    (place_param_targets[redirect_index]
					     ->unparametrized().get_name_nondynamic(), ">")); 
				throw ERROR_LOGICAL;
			}
			place_output= place_output_new; 
			assert(! place_output.empty()); 
			redirect_index= place_param_targets.size(); 
		}

		string param_1, param_2;
		if (! target_name->valid(param_1, param_2)) {
			place_target <<
				fmt("the two parameters %s and %s in the name %s "
				    "must be separated by at least one character",
				    name_format_word('$' + param_1),
				    name_format_word('$' + param_2),
				    target_name->format_word()); 
			explain_separated_parameters(); 
			throw ERROR_LOGICAL;
		}

		string parameter_duplicate;
		if ((parameter_duplicate= target_name->get_duplicate_parameter()) != "") {
			place_target <<
				fmt("target %s must not contain duplicate parameter %s", 
				    target_name->format_word(),
				    prefix_format_word(parameter_duplicate, "$")); 
			throw ERROR_LOGICAL;
		}

		shared_ptr <const Place_Param_Target> place_param_target= make_shared <Place_Param_Target>
			(flags_type, *target_name, place_target);

		place_param_targets.push_back(place_param_target); 
	}

	if (place_param_targets.size() == 0) {
		assert(iter == iter_begin); 
		return nullptr; 
	}

	/* Check that all targets have the same set of parameters */ 
	set <string> parameters_0;
	for (const string &parameter:  place_param_targets[0]->place_name.get_parameters()) {
		parameters_0.insert(parameter); 
	}
	assert(place_param_targets.size() >= 1); 
	for (size_t i= 1;  i < place_param_targets.size();  ++i) {
		set <string> parameters_i;
		for (const string &parameter:  
			     place_param_targets[i]->place_name.get_parameters()) {
			parameters_i.insert(parameter); 
		}
		if (parameters_i != parameters_0) {
			place_param_targets[i]->place <<
				fmt("parameters of target %s differ", 
				    place_param_targets[i]->format_word());
			place_param_targets[0]->place <<
				fmt("from parameters of target %s in rule with multiple targets",
				    place_param_targets[0]->format_word()); 
			throw ERROR_LOGICAL;
		}
	}

	if (iter == tokens.end()) {
		place_end << 
			fmt("expected a command, %s, %s, or %s",
			    char_format_word(':'), char_format_word(';'), char_format_word('=')); 
		place_param_targets.back()->place
			<< fmt("after target %s", 
			       place_param_targets.back()->format_word()); 
		throw ERROR_LOGICAL;
	}

	vector <shared_ptr <const Dep> > deps;

	bool had_colon= false;

	/* Empty at first */ 
	Place_Name filename_input;
	Place place_input;

	if (is_operator(':')) {
		had_colon= true; 
		++iter; 
		parse_expression_list(deps, 
				      filename_input, 
				      place_input, 
				      place_param_targets); 
	} 

	/* Command */ 
	if (iter == tokens.end()) {
		if (had_colon)
			place_end << fmt("expected a dependency, a command, or %s",
					 char_format_word(';'));
		else
			place_end << fmt("expected a command, %s, %s, or %s",
					 char_format_word(';'), 
					 char_format_word(':'),
					 char_format_word('='));
		place_param_targets[0]->place
			<< fmt("for target %s", place_param_targets[0]->format_word());
		throw ERROR_LOGICAL;
	}

	shared_ptr <Command> command;
	/* Remains null when there is no command */ 

	bool is_hardcode;
	/* When command is not null, whether the command is a command or
	 * hardcoded content */

	Place place_nocommand; 
	/* Place of ';' */ 

	Place place_equal;
	/* Place of '=' */

	shared_ptr <Name_Token> name_copy;
	/* Name of the copy-from file */ 

	if ((command= is <Command> ())) {
		++iter; 
		is_hardcode= false;
	} else if (! had_colon && is_operator('=')) {
		place_equal= (*iter)->get_place(); 
		++iter;

		if (iter == tokens.end()) {
			place_end << fmt("expected a filename, a flag, or %s",
					 char_format_word('{')); 
			place_equal << fmt("after %s", 
					    char_format_word('=')); 
			throw ERROR_LOGICAL;
		}

		if ((command= is <Command> ())) {
			/* Hardcoded content */ 
			++iter; 
			assert(place_param_targets.size() != 0); 
			if (place_param_targets.size() != 1) {
				place_equal << 
					fmt("there must not be assigned content using %s",
					    char_format_word('=')); 
				place_param_targets[0]->place << 
					fmt("in rule for %s... with multiple targets",
					    place_param_targets[0]->format_word()); 
					    
				throw ERROR_LOGICAL; 
			}
			if ((place_param_targets[0]->flags & F_TARGET_TRANSIENT)) {
				place_equal << 
					fmt("there must not be assigned content using %s",
					    char_format_word('=')); 
				place_param_targets[0]->place <<
					fmt("for transient target %s", 
					    place_param_targets[0]->format_word()); 
				throw ERROR_LOGICAL; 
			}
			/* No redirected output is checked later */ 
			is_hardcode= true; 
		} else {
			Place place_flag_persistent;
			Place place_flag_optional; 

			while (is <Flag_Token> ()) {
				shared_ptr <Flag_Token> flag= is <Flag_Token> ();
				if (flag->flag == 'p') {
					place_flag_persistent= flag->get_place();
					++iter;
				} else if (flag->flag == 'o') {
					if (! option_nonoptional) 
						place_flag_optional= flag->get_place(); 
					++iter;
				} else {
					flag->get_place()
						<< fmt("flag %s must not be used",
						       multichar_format_word
						       (frmt("-%c", flag->flag))); 
					place_equal << 
						fmt("in copy rule using %s for target %s", 
						    char_format_word('='),
						    place_param_targets[0]->format_word()); 
					throw ERROR_LOGICAL;
				}
			}

			if (! is <Name_Token> ()) {
				if (iter == tokens.end()) {
					(*iter)->get_place_start() << 
						fmt("expected a filename, a flag, or %s", 
						    char_format_word('{')); 
				} else {
					(*iter)->get_place_start() << 
						fmt("expected a filename, a flag, or %s, not %s", 
						    char_format_word('{'),
						    (*iter)->format_start_word()); 
				}
				place_equal << fmt("after %s", char_format_word('='));
				throw ERROR_LOGICAL;
			}

			/* Copy rule */ 
			name_copy= is <Name_Token> (); 
			++iter;

			/* Check that the source file contains
			 * only parameters that also appear in
			 * the target  */
			set <string> parameters;
			for (auto &parameter:
				     place_param_targets[0]->place_name.get_parameters()) {
				parameters.insert(parameter); 
			}
			for (size_t jj= 0;  jj < name_copy->get_n();  ++jj) {
				string parameter= 
					name_copy->get_parameters()[jj]; 
				if (parameters.count(parameter) == 0) {
					name_copy->places[jj] <<
						fmt("parameter %s must not appear in copied file %s", 
						    prefix_format_word(parameter, "$"), 
						    name_copy->format_word());
					place_param_targets[0]->place << 
						fmt("because it does not appear in target %s",
						    place_param_targets[0]->format_word());
					throw ERROR_LOGICAL;
				}
			}

			if (iter == tokens.end()) {
				place_end << fmt("expected %s", char_format_word(';'));
				name_copy->get_place() << 
					fmt("after copy dependency %s",
					    name_copy->format_word()); 
				throw ERROR_LOGICAL; 
			}
			if (! is_operator(';')) {
				(*iter)->get_place() <<
					fmt("expected %s", char_format_word(';'));
				name_copy->place << 
					fmt("after copy dependency %s",
					    name_copy->format_word()); 
				throw ERROR_LOGICAL; 
			}
			++iter;

			if (! place_output.empty()) {
				place_output << 
					fmt("output redirection using %s must not be used",
					    char_format_word('>'));
				place_equal << 
					fmt("in copy rule using %s for target %s", 
					    char_format_word('='),
					    place_param_targets[0]->format_word()); 
				throw ERROR_LOGICAL;
			}

			/* Check that there is just a single
			 * target */
			if (place_param_targets.size() != 1) {
				place_equal <<
					fmt("there must not be a copy rule using %s",
					    char_format_word('=')); 
				place_param_targets[0]->place << 
					fmt("for multiple targets %s...",
					    place_param_targets[0]->format_word()); 
				throw ERROR_LOGICAL; 
			}

			/* Check that target is not transient */ 
			if (place_param_targets[0]->flags & F_TARGET_TRANSIENT) {
				assert(place_param_targets[0]->flags & F_TARGET_TRANSIENT); 
				place_equal << fmt("copy rule using %s cannot be used",
						   char_format_word('='));
				place_param_targets[0]->place 
					<< fmt("with transient target %s",
					       place_param_targets[0]->format_word()); 
				throw ERROR_LOGICAL;
			}

			assert(place_param_targets.size() == 1); 

			/* Append target name when source ends
			 * in slash */
			append_copy(*name_copy, place_param_targets[0]->place_name); 

			return make_shared <const Rule> (place_param_targets[0], name_copy,
							 place_flag_persistent,
							 place_flag_optional);
		}
		
	} else if (is_operator(';')) {
		place_nocommand= (*iter)->get_place(); 
		++iter;
	} else {
		(*iter)->get_place() <<
			(had_colon
			 ? fmt("expected a dependency, a command, or %s, not %s", 
			       char_format_word(';'),
			       (*iter)->format_start_word())
			 : fmt("expected a command, %s, %s, or %s, not %s",
			       char_format_word(':'),
			       char_format_word(';'),
			       char_format_word('='),
			       (*iter)->format_start_word()));
		place_param_targets[0]->place <<
			fmt("for target %s", 
			    place_param_targets[0]->format_word());
		throw ERROR_LOGICAL;
	}

	/* Cases where output redirection is not possible */ 
	if (! place_output.empty()) {
		/* Already checked before */ 
		assert((place_param_targets[redirect_index]->flags & F_TARGET_TRANSIENT) == 0); 

		if (command == nullptr) {
			place_output << 
				fmt("output redirection using %s must not be used",
				     char_format_word('>'));
			place_nocommand <<
				fmt("in rule for %s without a command",
				    place_param_targets[0]->format_word());
			throw ERROR_LOGICAL;
		}

		if (command != nullptr && is_hardcode) {
			place_output <<
				fmt("output redirection using %s must not be used",
				     char_format_word('>'));
			place_equal <<
				fmt("in rule for %s with assigned content using %s",
				    place_param_targets[0]->format_word(),
				    char_format_word('=')); 
			throw ERROR_LOGICAL;
		}
	}

	/* Cases where input redirection is not possible */ 
	if (! filename_input.empty()) {
		if (command == nullptr) {
			place_input <<
				fmt("input redirection using %s must not be used",
				     char_format_word('<'));
			place_nocommand <<
				fmt("in rule for %s without a command",
				    place_param_targets[0]->format_word()); 
			throw ERROR_LOGICAL;
		} else {
			assert(! is_hardcode); 
		}
	}

	return make_shared <const Rule> 
		(move(place_param_targets), 
		 deps, 
		 command, is_hardcode, 
		 redirect_index,
		 filename_input);
}

bool Parser::parse_expression_list(vector <shared_ptr <const Dep> > &ret, 
				   Place_Name &place_name_input,
				   Place &place_input,
				   const vector <shared_ptr <const Place_Param_Target> > &targets)
{
	assert(ret.size() == 0);

	while (iter != tokens.end()) {
		shared_ptr <const Dep> ret_new; 
		bool r= parse_expression(ret_new, 
					 place_name_input, 
					 place_input, targets);
		if (!r) {
			assert(ret_new == nullptr); 
			return ! ret.empty(); 
		}
		assert(ret_new != nullptr); 
		ret.push_back(ret_new); 
	}

	return ! ret.empty(); 
}

bool Parser::parse_expression(shared_ptr <const Dep> &ret,
			      Place_Name &place_name_input,
			      Place &place_input,
			      const vector <shared_ptr <const Place_Param_Target> > &targets)
{
	assert(ret == nullptr); 

	/* '(' expression* ')' */ 
	if (is_operator('(')) {
		Place place_paren= (*iter)->get_place();
		++iter;
		vector <shared_ptr <const Dep> > r;
		if (parse_expression_list(r, place_name_input, place_input, targets)) {
			assert(r.size() >= 1); 
			if (r.size() > 1) {
				ret= make_shared <Compound_Dep> (move(r), place_paren); 
			} else {
				ret= move(r.at(0)); 
			}
			r.clear(); 
		}
		if (iter == tokens.end()) {
			place_end << fmt("expected %s", char_format_word(')'));
			place_paren << fmt("after opening %s", 
					    char_format_word('(')); 
			throw ERROR_LOGICAL;
		}
		if (! is_operator(')')) {
			(*iter)->get_place_start() << 
				fmt("expected %s, not %s", 
				    char_format_word(')'),
				    (*iter)->format_start_word());
			place_paren << fmt("after opening %s",
					    char_format_word('(')); 
			throw ERROR_LOGICAL;
		}
		++ iter; 

		/* If RET is null, it means we had empty parentheses.
		 * Return an empty Compound_Dependency in that case  */ 
		if (ret == nullptr) {
			ret= make_shared <Compound_Dep> (place_paren); 
		}

		if (next_concatenates()) {
			shared_ptr <const Dep> next;
			bool rr= parse_expression(next, place_name_input, place_input, targets);
			/* It can be that an empty list was parsed, in
			 * which case RR is true but the list is empty */
			if (rr && next != nullptr) {
				shared_ptr <Concat_Dep> ret_new= make_shared <Concat_Dep> ();
				ret_new->push_back(ret);
				ret_new->push_back(next);
				ret.reset();
				ret= move(ret_new); 
			}
		}

		return true; 
	} 

	/* '[' expression* ']' */
	if (is_operator('[')) {
		Place place_bracket= (*iter)->get_place(); 
		++iter;	
		vector <shared_ptr <const Dep> > r2;
		parse_expression_list(r2, place_name_input, place_input, targets);

		if (iter == tokens.end()) {
			place_end << fmt("expected %s", char_format_word(']'));
			place_bracket << fmt("after opening %s", 
					      char_format_word('[')); 
			throw ERROR_LOGICAL;
		}
		if (! is_operator(']')) {
			(*iter)->get_place_start() << 
				fmt("expected %s, not %s", 
				    char_format_word(']'),
				    (*iter)->format_start_word());
			place_bracket << fmt("after opening %s",
					      char_format_word('[')); 
			throw ERROR_LOGICAL;
		}
		++ iter; 
		shared_ptr <Compound_Dep> ret_nondynamic= 
			make_shared <Compound_Dep> (place_bracket); 
		for (auto &j:  r2) {
			
			/* Variable dependency cannot appear within
			 * dynamic dependency */ 
			if (j->flags & F_VARIABLE) {
				j->get_place() <<
					fmt("variable dependency %s must not appear", 
					    j->format_word()); 
				place_bracket <<
					fmt("within dynamic dependency started by %s",
					     char_format_word('[')); 
				throw ERROR_LOGICAL; 
			}

			ret_nondynamic->push_back(j);
		}
		ret= make_shared <Dynamic_Dep> (0, ret_nondynamic); 

		if (next_concatenates()) {
			shared_ptr <const Dep> next;
			bool rr= parse_expression(next, place_name_input, place_input, targets);
			/* It can be that an empty list was parsed, in
			 * which case RR is true but the list is empty */
			if (rr && next != nullptr) {
				shared_ptr <Concat_Dep> ret_new=
					make_shared <Concat_Dep> ();
				ret_new->push_back(ret);
				ret_new->push_back(next);
				ret.reset();
				ret= move(ret_new); 
			}
		}

		/* If RET is null, it means we had empty parentheses.
		 * Return an empty Compound_Dependency in that case  */ 
		if (ret == nullptr) {
			ret= make_shared <Compound_Dep> (place_bracket); 
		}

		return true; 
	} 

	/* flag expression */ 
	if (is <Flag_Token> ()) {
		const Flag_Token &flag_token= *is <Flag_Token> (); 
 		const Place place_flag= (*iter)->get_place();
		const unsigned i_flag= flag_get_index(flag_token.flag); 
 		++iter; 

		if (! parse_expression(ret, place_name_input, place_input, targets)) {
			if (iter == tokens.end()) {
				place_end << "expected a dependency";
			} else {
				(*iter)->get_place_start() << 
					fmt("expected a dependency, not %s",
					    (*iter)->format_start_word());
			}
			place_flag << fmt("after flag %s",
					  multichar_format_word(frmt("-%c", flag_token.flag))); 
			throw ERROR_LOGICAL;
		}

		/* A dependency cannot be an input dependency and
		 * optional at the same time.  Note: Input redirection
		 * must not appear in dynamic dependencies, and
		 * therefore it is sufficient to check this here.  */   
		if (! place_name_input.place.empty() && flag_token.flag == 'o') {
			place_input <<
				fmt("input redirection using %s must not be used",
				    char_format_word('<')); 
			place_flag <<
				fmt("in conjunction with optional dependency flag %s",
				    multichar_format_word("-o")); 
			throw ERROR_LOGICAL;
		}

		/* Add the flag */ 
		if (! ((i_flag == I_OPTIONAL && option_nonoptional) ||
		       (i_flag == I_TRIVIAL  && option_nontrivial))) {
			shared_ptr <Dep> ret_new= Dep::clone(ret);
			ret_new->flags |= (1 << i_flag); 
			assert(i_flag < C_WORD); 
			if (i_flag < C_PLACED)
				ret_new->set_place_flag(i_flag, place_flag); 
			ret= ret_new; 
		}

		return true;
	}

	/* '$' ; variable dependency */ 
	shared_ptr <const Dep> dep= 
		parse_variable_dep(place_name_input, place_input, targets);
	if (dep != nullptr) {
		ret= dep; 
		return true; 
	}

	/* Redirect dependency */
	dep= parse_redirect_dep(place_name_input, place_input, targets); 
	if (dep != nullptr) {
		ret= dep;
		return true; 
	}

	return false;
}

shared_ptr <const Dep> Parser
::parse_variable_dep(Place_Name &place_name_input, 
		     Place &place_input,
		     const vector <shared_ptr <const Place_Param_Target> > &targets)
{
	bool has_input= false;

	shared_ptr <const Dep> ret;

	if (! is_operator('$')) 
		return nullptr;

	const Place place_dollar= (*iter)->get_place();
	++iter;

	if (iter == tokens.end()) {
		place_end << fmt("expected %s", char_format_word('['));
		place_dollar << fmt("after %s", 
				    char_format_word('$')); 
		throw ERROR_LOGICAL;
	}
	
	if (! is_operator('[')) {
		/* The '$' and '[' operators are only generated when
		 * they both appear in conjunction. */ 
		assert(false);
		return nullptr;
	}

	++iter;


	/* Flags */ 
	Flags flags= F_VARIABLE;
	Place places_flags[C_PLACED]; 
	for (unsigned i= 0;  i < C_PLACED;  ++i)
		places_flags[i].clear(); 
	Place place_flag_last;
	char flag_last= '\0';
	while (is_flag('p') || is_flag('o') || is_flag('t')) {

		flag_last= is <Flag_Token> ()->flag; 
		place_flag_last= (*iter)->get_place();
		if (is_flag('p')) {
			flags |= F_PERSISTENT; 
			places_flags[I_PERSISTENT]= place_flag_last; 
		} else if (is_flag('o')) {
			/* If the nonoptional (-g) option is set, ignore the -o flag */
			if (! option_nonoptional) {
				(*iter)->get_place() << 
					fmt("optional dependency using %s must not appear",
					     multichar_format_word("-o")); 
				place_dollar << "within dynamic variable declaration";
				throw ERROR_LOGICAL; 
			}
		} else if (is_flag('t')) {
			if (! option_nontrivial) {
				flags |= F_TRIVIAL; 
				places_flags[I_TRIVIAL]= place_flag_last; 
			}
		} else assert(false);
		++iter;
	}

	/* Input redirection using '<' */ 
	if (is_operator('<')) {
		has_input= true;
		place_input= (*iter)->get_place(); 
		flags |= F_INPUT; 
		++iter;
	}
	
	/* Name of variable dependency */ 
	if (! is <Name_Token> ()) {
		(*iter)->get_place_start() <<
			fmt("expected a filename, not %s",
			    (*iter)->format_start_word()); 
		if (has_input)
			place_input << fmt("after %s", 
					    char_format_word('<')); 
		else if (! place_flag_last.empty()) {
			assert(flag_last != '\0');
			place_flag_last << fmt("after %s", 
					       char_format_word(flag_last)); 
		} else
			place_dollar << fmt("after %s",
					    multichar_format_word("$[")); 

		throw ERROR_LOGICAL;
	}
	shared_ptr <Place_Name> place_name= is <Name_Token> ();
	++iter;

	if (has_input && ! place_name_input.empty()) {
		place_name->place << 
			fmt("there must not be a second input redirection %s", 
			    prefix_format_word(place_name->raw(), "<")); 
		place_name_input.place << 
			fmt("shadowing previous input redirection %s<%s%s", 
			    prefix_format_word(place_name_input.raw(), "<")); 
		if (targets.size() == 1) {
			targets.front()->place <<
				fmt("for target %s", targets.front()->format_word()); 
		} else if (targets.size() > 1) {
			targets.front()->place <<
				fmt("for targets %s...", targets.front()->format_word()); 
		}
		throw ERROR_LOGICAL;
	}

	/* Check that the name does not contain '=' */ 
	for (auto &j:  place_name->get_texts()) {
		if (j.find('=') != string::npos) {
			place_name->place <<
				fmt("name of variable dependency %s must not contain %s",
				    place_name->format_word(),
				    char_format_word('=')); 
			explain_variable_equal(); 
			throw ERROR_LOGICAL;
		}
	}


	if (iter == tokens.end()) {
		place_end << fmt("expected %s", char_format_word(']'));
		place_dollar << fmt("after opening %s",
				    multichar_format_word("$[")); 
		throw ERROR_LOGICAL;
	}

	/* Explicit variable name */ 
	string variable_name= "";
	if (is_operator('=')) {
		Place place_equal= (*iter)->get_place();
		++iter;
		if (iter == tokens.end()) {
			place_end << "expected a filename";
			place_equal << 
				fmt("after %s in variable dependency %s",
				    char_format_word('='),
				    place_name->format_word()); 
			throw ERROR_LOGICAL;
		}
		if (! is <Name_Token> ()) {
			(*iter)->get_place_start() << 
				fmt("expected a filename, not %s",
				    (*iter)->format_start_word());
			place_equal << fmt("after %s in variable dependency %s",
					   char_format_word('='),
					   place_name->format_word()); 
			throw ERROR_LOGICAL;
		}

		if (place_name->get_n() != 0) {
			place_name->place << 
				fmt("variable name %s must be unparametrized", 
				    place_name->format_word());
			throw ERROR_LOGICAL; 
		}

		variable_name= place_name->unparametrized();

		place_name= is <Name_Token> ();
		++iter; 
	}

	/* Closing ']' */ 
	if (! is_operator(']')) {
		(*iter)->get_place_start() << 
			fmt("expected %s, not %s", 
			    char_format_word(']'),
			    (*iter)->format_start_word());
		place_dollar << fmt("after opening %s",
				    multichar_format_word("$[")); 
		throw ERROR_LOGICAL;
	}
	++iter;

	if (has_input) {
		place_name_input= *place_name;
	}

	/* The place of the variable dependency as a whole is set on the
	 * name contained in it.  It would be conceivable to also set it
	 * on the dollar sign.  */
	return make_shared <Plain_Dep> 
		(flags, 
		 places_flags,
		 Place_Param_Target(0, *place_name, 
				    place_name->place), 
		 variable_name);
}

shared_ptr <const Dep> Parser::parse_redirect_dep
(Place_Name &place_name_input,
 Place &place_input,
 const vector <shared_ptr <const Place_Param_Target> > &targets)
{
	(void) targets;

	bool has_input= false;

	if (is_operator('<')) {
		has_input= true; 
		place_input= (*iter)->get_place();
		assert(! place_input.empty()); 
		++iter;
	}

	bool has_transient= false;
	Place place_at; 
	if (is_operator('@')) {
		place_at= (*iter)->get_place();
		if (has_input) {
			place_at << fmt("expected a filename, not %s",
					char_format_word('@')); 
			place_input << fmt("after input redirection using %s",
					    char_format_word('<')); 
			throw ERROR_LOGICAL;
		}
		++ iter;
		has_transient= true; 
	}

	if (iter == tokens.end()) {
		if (has_input) {
			place_end << "expected a filename";
			place_input << fmt("after input redirection using %s",
					    char_format_word('<')); 
			throw ERROR_LOGICAL;
		} else if (has_transient) {
			place_end << "expected the name of a transient target";
			place_at << fmt("after %s",
					 char_format_word('@')); 
			throw ERROR_LOGICAL; 
		} else {
			return nullptr;
		}
	}

	if (! is <Name_Token> ()) {
		if (has_input) {
			(*iter)->get_place_start() << 
				fmt("expected a filename, not %s",
				    (*iter)->format_start_word());
			place_input << fmt("after input redirection using %s",
					    char_format_word('<')); 
			throw ERROR_LOGICAL;
		} else if (has_transient) {
			(*iter)->get_place_start() 
				<< fmt("expected the name of a transient target, not %s",
				       (*iter)->format_start_word()); 
			place_at << fmt("after %s",
					 char_format_word('@')); 
			throw ERROR_LOGICAL;
		} else {
			return nullptr;
		}
	}

	shared_ptr <Name_Token> name_token= is <Name_Token> ();
	++iter; 

	if (has_input && ! place_name_input.empty()) {
		name_token->place << 
			fmt("there must not be a second input redirection %s", 
			    prefix_format_word(name_token->raw(), "<")); 
		place_name_input.place << 
			fmt("shadowing previous input redirection %s", 
			    prefix_format_word(place_name_input.raw(), "<")); 
		if (targets.size() == 1) {
			targets.front()->place <<
				fmt("for target %s", targets.front()->format_word()); 
		} else if (targets.size() > 1) {
			targets.front()->place <<
				fmt("for targets %s...", targets.front()->format_word()); 
		}
		throw ERROR_LOGICAL;
	}

	Flags flags= 0;
	if (has_input) {
		assert(place_name_input.empty()); 
		place_name_input= *name_token;
		flags |= F_INPUT;
	}

	if (! place_name_input.empty()) {
		assert(! place_input.empty()); 
	}

	Flags transient_bit= has_transient ? F_TARGET_TRANSIENT : 0;
	shared_ptr <const Dep> ret= make_shared <Plain_Dep>
		(flags | transient_bit,
		 Place_Param_Target(transient_bit,
				    *name_token,
				    has_transient ? place_at : name_token->place)); 

	if (next_concatenates()) {
		shared_ptr <const Dep> next;
		bool rr= parse_expression(next, place_name_input, place_input, targets);
		/* It can be that an empty list was parsed, in
		 * which case RR is true but the list is empty */
		if (rr && next != nullptr) {
			shared_ptr <Concat_Dep> ret_new=
				make_shared <Concat_Dep> ();
			ret_new->push_back(ret);
			ret_new->push_back(next);
			ret.reset();
			ret= move(ret_new); 
		}
	}

	return ret; 
}

void Parser::append_copy(      Name &to,
			 const Name &from) 
{
	/* Only append if TO ends in a slash */
	if (! (to.last_text().size() != 0 &&
	       to.last_text().back() == '/')) {
		return;
	}

	for (ssize_t i= from.get_n();  i >= 0;  --i) {
		for (ssize_t j= from.get_texts()[i].size() - 1;
		     j >= 0;  --j) {
			if (from.get_texts()[i][j] == '/') {

				/* Don't append the found slash, as TO
				 * already ends in a slash */ 
				to.append_text(from.get_texts()[i].substr(j + 1));

				for (size_t k= i;  k < from.get_n();  ++k) {
					to.append_parameter(from.get_parameters()[k]);
					to.append_text(from.get_texts()[k + 1]);
				}
				return;
			}
		}
	} 

	/* FROM does not contain slashes;
	 * prepend the whole FROM to TO */
	to.append(from);
}

void Parser::get_rule_list(vector <shared_ptr <const Rule> > &rules,
			  vector <shared_ptr <Token> > &tokens,
			  const Place &place_end)
{
	auto iter= tokens.begin(); 

	Parser parser(tokens, iter, place_end);

	parser.parse_rule_list(rules); 

	if (iter != tokens.end()) {
		(*iter)->get_place_start() 
			<< fmt("expected a rule, not %s", 
			       (*iter)->format_start_word()); 
		throw ERROR_LOGICAL;
	}
}

void Parser::get_expression_list(vector <shared_ptr <const Dep> > &deps,
				vector <shared_ptr <Token> > &tokens,
				const Place &place_end,
				Place_Name &input,
				Place &place_input)
{
	auto iter= tokens.begin(); 
	Parser parser(tokens, iter, place_end); 
	vector <shared_ptr <const Place_Param_Target>> targets;
	parser.parse_expression_list(deps, input, place_input, targets); 
	if (iter != tokens.end()) {
		(*iter)->get_place_start() 
			<< fmt("expected a dependency, not %s",
			       (*iter)->format_start_word());
		throw ERROR_LOGICAL;
	}
}

shared_ptr <const Dep> Parser::get_target_dep(string text, const Place &place)
/*
 * This syntax supports only the characters '@' and '[]', and a single
 * name, without whitespace.  Thus, the syntax is:
 *
 *         '['^n [@] NAME ']'^n
 */
{
	assert(text != ""); 

	const char *begin= text.c_str();
	const char *p= text.c_str() + text.size();
	int closing= 0;
	while (p != begin && p[-1] == ']') {
		++closing;
		--p;
	}
	const char *end_name= p;

	const char *q= begin;
	while (q != end_name && *q == '[') {
		++q;
	}

	assert(q <= end_name); 

	/* For catching porting errors, flag this error separately */ 
	if (q != end_name && (*q == '!' || *q == '?')) {
		if (*q == '!') {
			place <<
				fmt("character %s cannot be used to denote persistent dependencies",
				    char_format_word('!')); 
		} else if (*q == '?') {
			place <<
				fmt("character %s cannot be used to denote optional dependencies",
				    char_format_word('?')); 
		} else  
			assert(false);
		throw ERROR_LOGICAL; 
	}

	Flags flags_type= 0; 
	const char *begin_name= q;
	if (begin_name != end_name && *q == '@') {
		flags_type= F_TARGET_TRANSIENT; 
		++ begin_name;
	}

	if (begin_name == end_name) {
		place << fmt("%s: name must not be empty",
			     name_format_word(text));
		throw ERROR_LOGICAL; 
	}

	shared_ptr <const Dep> ret= make_shared <Plain_Dep> 
		(flags_type, Place_Param_Target
		 (flags_type, 
		  Place_Name
		  (string(begin_name, end_name - begin_name), 
		   place)));

	while (q != begin) {
		if (q[-1] == '[') {
			ret= make_shared <Dynamic_Dep> (0, ret);
			-- closing;
		} else {
			assert(false); 
			/* Ignore the character */ 
		}
		--q;
	}

	assert(q == begin);
	
	if (closing != 0) {
		place << fmt("%s: unbalanced brackets %s[]%s", 
			     name_format_word(text),
			     Color::word, Color::end);
		throw ERROR_LOGICAL;
	}

	return ret; 
}

void Parser::print_separation_message(shared_ptr <const Token> token)
{
	string text;

	if (dynamic_pointer_cast <const Name_Token> (token)) {
		text= fmt("token %s",
			  dynamic_pointer_cast <const Name_Token> (token)->format_word()); 
	} else if (dynamic_pointer_cast <const Operator> (token)) {
		text= dynamic_pointer_cast <const Operator> (token)->format_long_word(); 
	} else {
		assert(false);
	}

	token->get_place() << 
		fmt("to separate it from %s", text);
}

bool Parser::next_concatenates() const
{
	if (iter == tokens.end())
		return false;

	if ((*iter)->whitespace)
		return false;

	if (is <Name_Token> ())
		return true;

	if (! is <Operator> ())
		return false;

	char op= is <Operator> ()->op;

	return op == '(' || op == '['; 
}

#endif /* ! PARSER_HH */
