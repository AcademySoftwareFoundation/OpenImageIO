# The PARSE_ARGUMENTS macro will take the arguments of another macro and define
# several variables. The first argument to PARSE_ARGUMENTS is a prefix to put
# on all variables it creates. The second argument is a list of names, and the
# third argument is a list of options. Both of these lists should be quoted.
# The rest of the PARSE_ARGUMENTS args are arguments from another macro to be
# parsed.
# 
#     PARSE_ARGUMENTS(prefix arg_names options arg1 arg2...) 
# 
# For each item in options, PARSE_ARGUMENTS will create a variable with that
# name, prefixed with prefix_. So, for example, if prefix is MY_MACRO and
# options is OPTION1;OPTION2, then PARSE_ARGUMENTS will create the variables
# MY_MACRO_OPTION1 and MY_MACRO_OPTION2. These variables will be set to true if
# the option exists in the command line or false otherwise.
# 
# For each item in arg_names, PARSE_ARGUMENTS will create a variable with that
# name, prefixed with prefix_. Each variable will be filled with the arguments
# that occur after the given arg_name is encountered up to the next arg_name or
# the end of the arguments. All options are removed from these lists.
# PARSE_ARGUMENTS also creates a prefix_DEFAULT_ARGS variable containing the
# list of all arguments up to the first arg_name encountered. 
#
# Downloaded from: http://www.itk.org/Wiki/CMakeMacroParseArguments
#
MACRO(PARSE_ARGUMENTS prefix arg_names option_names)
  SET(DEFAULT_ARGS)
  FOREACH(arg_name ${arg_names})    
    SET(${prefix}_${arg_name})
  ENDFOREACH(arg_name)
  FOREACH(option ${option_names})
    SET(${prefix}_${option} FALSE)
  ENDFOREACH(option)

  SET(current_arg_name DEFAULT_ARGS)
  SET(current_arg_list)
  FOREACH(arg ${ARGN})            
    SET(larg_names ${arg_names})    
    LIST(FIND larg_names "${arg}" is_arg_name)                   
    IF (is_arg_name GREATER -1)
      SET(${prefix}_${current_arg_name} ${current_arg_list})
      SET(current_arg_name ${arg})
      SET(current_arg_list)
    ELSE (is_arg_name GREATER -1)
      SET(loption_names ${option_names})    
      LIST(FIND loption_names "${arg}" is_option)            
      IF (is_option GREATER -1)
         SET(${prefix}_${arg} TRUE)
      ELSE (is_option GREATER -1)
         SET(current_arg_list ${current_arg_list} ${arg})
      ENDIF (is_option GREATER -1)
    ENDIF (is_arg_name GREATER -1)
  ENDFOREACH(arg)
  SET(${prefix}_${current_arg_name} ${current_arg_list})
ENDMACRO(PARSE_ARGUMENTS)



# Macro to set a variable with our funny overrides:
# If the variable is already set (by -D on the command line), leave it alone.
# If an environment variable of the same name is set, use that value
# (making it super easy for sites to override external tool locations).
# If neither of those, then use the default passed.
MACRO (setup_string name defaultval explanation)
    # If the named variable already has a value (was set by -D...), leave
    # it alone.  But if it's not yet set...
    IF ( "${${name}}" STREQUAL "" )
        # If there's an environment variable of the same name that's
        # nonempty, use the env variable.  Otherwise, use the default.
        IF ( NOT $ENV{${name}} STREQUAL "" )
            SET ( ${name} $ENV{${name}} )
                  # CACHE STRING ${explanation} )
        ELSE ()
            SET ( ${name} ${defaultval} )
                  # CACHE STRING ${explanation} )
        ENDIF ()
    ENDIF ()
    MESSAGE ( STATUS "${name} = ${${name}}" )
ENDMACRO ()

MACRO (setup_path name defaultval explanation)
    # If the named variable already has a value (was set by -D...), leave
    # it alone.  But if it's not yet set...
    IF ( "${${name}}" STREQUAL "" )
        # If there's an environment variable of the same name that's
        # nonempty, use the env variable.  Otherwise, use the default.
        IF ( NOT $ENV{${name}} STREQUAL "" )
            SET ( ${name} $ENV{${name}} )
                  # CACHE PATH ${explanation} )
        ELSE ()
            SET ( ${name} ${defaultval} )
                  # CACHE PATH ${explanation} )
        ENDIF ()
    ENDIF ()
    MESSAGE ( STATUS "${name} = ${${name}}" )
ENDMACRO ()
