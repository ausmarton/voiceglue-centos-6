
/****************License************************************************
 * Vocalocity OpenVXI
 * Copyright (C) 2004-2005 by Vocalocity, Inc. All Rights Reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * Vocalocity, the Vocalocity logo, and VocalOS are trademarks or 
 * registered trademarks of Vocalocity, Inc. 
 * OpenVXI is a trademark of Scansoft, Inc. and used under license 
 * by Vocalocity.
 ***********************************************************************/

/*****************************************************************************
 *****************************************************************************
 *
 * Implementation of the SBjsi functions defined in SBjsiAPI.h,
 * see that header for details. These implementations are just a thin
 * C wrapper around the real implementation.
 *
 *****************************************************************************
 ****************************************************************************/


// -----1=0-------2=0-------3=0-------4=0-------5=0-------6=0-------7=0-------8


#include "SBjsiInternal.h"


#include "SBjsiLog.h"                // For logging
#include "JsiRuntime.hpp"            // For JsiRuntime class
#include "JsiContext.hpp"            // For JsiContext class
#include "SBjsiInterface.h"          // For SBjsiInterface

#include "SBjsiAPI.h"                // Header for these functions

#include <xercesc/dom/DOMDocument.hpp>

// Real VXIjsiContext API object
extern "C" {
typedef struct VXIjsiContext
{
  // JavaScript context object
  JsiContext *jsiContext;

} VXIjsiContext;
}

// Convenience macro
#define GET_SBJSI(pThis, context, rc) \
  VXIjsiResult rc = VXIjsi_RESULT_SUCCESS; \
  SBjsiInterface *sbJsi = (SBjsiInterface *) pThis; \
  if (( ! sbJsi ) || ( ! context )) { \
    if ( sbJsi ) SBjsiLogger::Error(sbJsi->log, MODULE_SBJSI, \
                                      JSI_ERROR_NULL_INTERFACE_PTR, NULL); \
    rc = VXIjsi_RESULT_INVALID_ARGUMENT; \
    return rc; \
  }


// -----1=0-------2=0-------3=0-------4=0-------5=0-------6=0-------7=0-------8


/**
 * Return the version
 */
extern "C" 
VXIint32 SBjsiGetVersion(void)
{
  return VXI_CURRENT_VERSION;
}


/**
 * Return the implementation name
 */
extern "C" 
const VXIchar* SBjsiGetImplementationName(void)
{
  return SBJSI_IMPLEMENTATION_NAME;
}


/**
 * Create and initialize a new script context
 *
 * This creates a new environment using a model, usually the global
 * environment created by VXIjsiInit(). Currently one context is
 * created per thread, but the implementation must support the ability
 * to have multiple contexts per thread.
 *
 * @param model    [IN]  Pointer to the model context that will be the
 *                       basis for the new context, pass NULL to use the
 *                       default environment
 * @param context  [OUT] Newly created context
 *
 * @result VXIjsiResult 0 on success 
 */
extern "C"
VXIjsiResult SBjsiCreateContext(VXIjsiInterface        *pThis,
				VXIjsiContext         **context)
{
  static const wchar_t func[] = L"SBjsiCreateContext";
  GET_SBJSI(pThis, context, rc);
  sbJsi->log->Diagnostic(sbJsi->log, sbJsi->diagTagBase + SBJSI_LOG_API,
			  func, L"entering: 0x%p", context);

  // Allocate the wrapper object and the new context
  *context = NULL;
  VXIjsiContext *newContext = new VXIjsiContext;
  if ( newContext == NULL ) {
    SBjsiLogger::Error(sbJsi->log, MODULE_SBJSI, JSI_ERROR_OUT_OF_MEMORY,
			 NULL);
    rc = VXIjsi_RESULT_OUT_OF_MEMORY;
  } else if ( (newContext->jsiContext = new JsiContext) == NULL ) {
    delete newContext;
    SBjsiLogger::Error(sbJsi->log, MODULE_SBJSI, JSI_ERROR_OUT_OF_MEMORY,
			 NULL);
    rc = VXIjsi_RESULT_OUT_OF_MEMORY;
    sbJsi->log->Diagnostic(sbJsi->log, sbJsi->diagTagBase + SBJSI_LOG_API, 
			    func, L"exiting: returned %d", rc);
    return rc;
  }

  // Now do the low-level creation
  rc = newContext->jsiContext->Create(sbJsi->jsiRuntime, sbJsi->contextSize,
				       sbJsi->maxBranches, sbJsi->log,
				       sbJsi->diagTagBase);
  if ( rc == VXIjsi_RESULT_SUCCESS ) {
    *context = newContext;
  } else {
    delete newContext->jsiContext;
    delete newContext;
  }

  sbJsi->log->Diagnostic(sbJsi->log, sbJsi->diagTagBase + SBJSI_LOG_API,
			  func, L"exiting: returned %d, 0x%p", rc, *context);
  return rc;
}


/**
 * Destroy a script context, clean up storage if required
 *
 * @param context  [IN] Context to destroy
 *
 * @result VXIjsiResult 0 on success
 */
extern "C"
VXIjsiResult SBjsiDestroyContext(VXIjsiInterface        *pThis,
				 VXIjsiContext         **context)
{
  static const wchar_t func[] = L"SBjsiDestroyContext";
  GET_SBJSI(pThis, context, rc);
  sbJsi->log->Diagnostic(sbJsi->log, sbJsi->diagTagBase + SBJSI_LOG_API, 
			  func, L"entering: 0x%p (0x%p)", 
			  context, (context ? *context : NULL));

  if ( *context == NULL ) {
    SBjsiLogger::Error(sbJsi->log, MODULE_SBJSI, JSI_ERROR_INVALID_ARG,
			 NULL);
    rc = VXIjsi_RESULT_INVALID_ARGUMENT;
  } else {
    delete (*context)->jsiContext;
    delete *context;
    *context = NULL;
  }

  sbJsi->log->Diagnostic(sbJsi->log, sbJsi->diagTagBase + SBJSI_LOG_API, func,
			  L"exiting: returned %d", rc);
  return rc;
}


/**
 * Create a script variable relative to the current scope, initialized
 *  to an expression
 *
 * NOTE: When there is an expression, the expression is evaluated,
 *  then the value of the evaluated expression (the final
 *  sub-expression) assigned. Thus an expression of "1; 2;" actually
 *  assigns 2 to the variable.
 *
 * @param context  [IN] JavaScript context to create the variable within
 * @param name     [IN] Name of the variable to create
 * @param expr     [IN] Expression to set the initial value of the variable 
 *                      (if NULL or empty the variable is set to JavaScript 
 *                      Undefined as required for VoiceXML 1.0 <var>)
 *
 * @result VXIjsiResult 0 on success 
 */
extern "C"
VXIjsiResult SBjsiCreateVarExpr(VXIjsiInterface        *pThis,
				VXIjsiContext          *context, 
				const VXIchar          *name, 
				const VXIchar          *expr)
{
  static const wchar_t func[] = L"SBjsiCreateVarExpr";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, 
			     L"entering: 0x%p, '%s', '%s'", 
			     context, name, expr);

  rc = context->jsiContext->CreateVar(name, expr);

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}

  
/**
 * Create a script variable relative to the current scope, initialized
 *  to a VXIValue based value
 *
 * @param context  [IN] JavaScript context to create the variable within
 * @param name     [IN] Name of the variable to create
 * @param value    [IN] VXIValue based value to set the initial value of 
 *                      the variable (if NULL the variable is set to 
 *                      JavaScript Undefined as required for VoiceXML 1.0
 *                      <var>). VXIMap is used to pass JavaScript objects.
 *
 * @result VXIjsiResult 0 on success 
 */
extern "C"
VXIjsiResult SBjsiCreateVarValue(VXIjsiInterface        *pThis,
				 VXIjsiContext          *context, 
				 const VXIchar          *name, 
				 const VXIValue         *value)
{
  static const wchar_t func[] = L"SBjsiCreateVarValue";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, 
			     L"entering: 0x%p, '%s', 0x%p", 
			     context, name, value);

  rc = context->jsiContext->CreateVar(name, value);

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}

  
/**
 * Set a script variable to an expression relative to the current scope
 *
 * NOTE: The expression is evaluated, then the value of the
 *  evaluated expression (the final sub-expression) assigned. Thus
 *  an expression of "1; 2;" actually assigns 2 to the variable.
 *
 * @param context  [IN] JavaScript context to set the variable within
 * @param name     [IN] Name of the variable to set
 * @param expr     [IN] Expression to be assigned
 *
 * @result VXIjsiResult 0 on success 
 */
extern "C"
VXIjsiResult SBjsiSetVarExpr(VXIjsiInterface        *pThis,
			     VXIjsiContext          *context, 
			     const VXIchar          *name, 
			     const VXIchar          *expr)
{
  static const wchar_t func[] = L"SBjsiSetVarExpr";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, 
			     L"entering: 0x%p, '%s', '%s'", 
			     context, name, expr);

  rc = context->jsiContext->SetVar(name, expr);

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}


/**
 * Set a script variable to a DOMDocument relative to the current scope
 *
 * NOTE: The expression is evaluated, then the value of the
 *  evaluated expression (the final sub-expression) assigned. Thus
 *  an expression of "1; 2;" actually assigns 2 to the variable.
 *
 * @param context  [IN] JavaScript context to set the variable within
 * @param name     [IN] Name of the variable to set
 * @param doc      [IN] DOMDocument to be assigned
 *
 * @result VXIjsiResult 0 on success 
 */
extern "C"
VXIjsiResult SBjsiCreateVarDOM(VXIjsiInterface        *pThis,
			     VXIjsiContext          *context, 
			     const VXIchar          *name, 
				 VXIPtr                 *doc )
{
  static const wchar_t func[] = L"SBjsiCreateVarDOM";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, 
			     L"entering: 0x%p, '%s'", 
			     context, name);

  xercesc::DOMDocument *domdoc = reinterpret_cast<xercesc::DOMDocument *>(VXIPtrValue(doc));
  rc = context->jsiContext->CreateVar(name, domdoc);

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}
  
/**
 * Set a script variable to a value relative to the current scope
 *
 * @param context  [IN] JavaScript context to set the variable within
 * @param name     [IN] Name of the variable to set
 * @param value    [IN] VXIValue based value to be assigned. VXIMap is 
 *                      used to pass JavaScript objects.
 *
 * @result VXIjsiResult 0 on success 
 */
extern "C"
VXIjsiResult SBjsiSetVarValue(VXIjsiInterface        *pThis,
			      VXIjsiContext          *context, 
			      const VXIchar          *name, 
			      const VXIValue         *value)
{
  static const wchar_t func[] = L"SBjsiSetVarValue";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, 
			     L"entering: 0x%p, '%s', 0x%p", 
			     context, name, value);

  rc = context->jsiContext->SetVar(name, value);

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}


/**
 * Get the value of a variable
 *
 * @param context  [IN]  JavaScript context to get the variable from
 * @param name     [IN]  Name of the variable to get
 * @param value    [OUT] Value of the variable, returned as the VXI
 *                       type that most closely matches the variable's
 *                       JavaScript type. This function allocates this
 *                       for return on success (returns a NULL pointer
 *                       otherwise), the caller is responsible for
 *                       destroying it via VXIValueDestroy(). VXIMap
 *                       is used to return JavaScript objects.
 *
 * @result VXIjsiResult 0 on success, VXIjsi_RESULT_FAILURE if the
 *         variable has a JavaScript value of Null,
 *         VXIjsi_RESULT_NON_FATAL_ERROR if the variable is not
 *         defined (JavaScript Undefined), or another error code for
 *         severe errors 
 */
extern "C"
VXIjsiResult SBjsiGetVar(VXIjsiInterface         *pThis,
			 const VXIjsiContext     *context, 
			 const VXIchar           *name,
			 VXIValue               **value)
{
  static const wchar_t func[] = L"SBjsiGetVar";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, 
			     L"entering: 0x%p, '%s', 0x%p", 
			     context, name, value);

  rc = context->jsiContext->GetVar(name, value);

  context->jsiContext->Diag(SBJSI_LOG_API, func, 
			     L"exiting: returned %d, 0x%p (0x%p)",
			     rc, value, (value ? *value : NULL));
  return rc;
}


/**
 * Check whether a variable is defined (not JavaScript Undefined)
 *
 * NOTE: A variable with a JavaScript Null value is considered defined
 *
 * @param context  [IN]  JavaScript context to check the variable in
 * @param name     [IN]  Name of the variable to check
 *
 * @result VXIjsiResult 0 on success (variable is defined),
 *         VXIjsi_RESULT_FAILURE if the variable is not defined,
 *         or another error code for severe errors
 */
extern "C"
VXIjsiResult SBjsiCheckVar(VXIjsiInterface        *pThis,
			   const VXIjsiContext    *context, 
			   const VXIchar          *name)
{
  static const wchar_t func[] = L"SBjsiCheckVar";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, L"entering: 0x%p, '%s'",
			     context, name);

  rc = context->jsiContext->CheckVar(name);

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}


/**
  * set a script variable read-only to the current scope
  *  
  * @param context  [IN] ECMAScript context in which the variable
  *                      has been created
  * @param name     [IN] Name of the variable to set as read only.
  *
  * @return VXIjsi_RESULT_SUCCESS on success
  */
extern "C"
VXIjsiResult SBjsiSetReadOnly(struct VXIjsiInterface *pThis,
                             VXIjsiContext    *context,
                             const VXIchar          *name)
{

  static const wchar_t func[] = L"SBjsiSetReadOnly";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, L"entering: 0x%p, '%s'",
			     context, name);

  rc = context->jsiContext->SetReadOnly(name);

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}                          

/**
 * Execute a script, optionally returning any execution result
 *
 * @param context  [IN]  JavaScript context to execute within
 * @param expr     [IN]  Buffer containing the script text
 * @param value    [OUT] Result of the script execution, returned 
 *                       as the VXI type that most closely matches 
 *                       the variable's JavaScript type. Pass NULL
 *                       if the result is not desired. Otherwise
 *                       this function allocates this for return on 
 *                       success when there is a return value (returns 
 *                       a NULL pointer otherwise), the caller is 
 *                       responsible for destroying it via 
 *                       VXIValueDestroy(). VXIMap is used to return 
 *                       JavaScript objects.
 *
 * @result VXIjsiResult 0 on success
 */
extern "C"
VXIjsiResult SBjsiEval(VXIjsiInterface         *pThis,
		       VXIjsiContext           *context,
		       const VXIchar           *expr,
		       VXIValue               **result)
{
  static const wchar_t func[] = L"SBjsiEval";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, 
			     L"entering: 0x%p, '%s', 0x%p", 
			     context, expr, result);

  rc = context->jsiContext->Eval(expr, result);

  context->jsiContext->Diag(SBJSI_LOG_API, func, 
			     L"exiting: returned %d, 0x%p (0x%p)", 
			     rc, result, (result ? *result : NULL));
  return rc;
}


/**
 * Push a new context onto the scope chain (add a nested scope)
 * @param context  [IN] JavaScript context to push the scope onto
 * @param name     [IN] Name of the scope, used to permit referencing
 *                      variables from an explicit scope within the
 *                      scope chain, such as "myscope.myvar" to access
 *                      "myvar" within a scope named "myscope"
 *
 * @result VXIjsiResult 0 on success
 */
extern "C"
VXIjsiResult SBjsiPushScope(VXIjsiInterface        *pThis,
			    VXIjsiContext          *context,
			    const VXIchar          *name,
			    const VXIjsiScopeAttr  attr)
{
  static const wchar_t func[] = L"SBjsiPushScope";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, L"entering: 0x%p, '%s'",
			     context, name);

  rc = context->jsiContext->PushScope(name, attr);

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}

/**
 * Pop a context from the scope chain (remove a nested scope)
 *
 * @param context  [IN] JavaScript context to pop the scope from
 *
 * @result VXIjsiResult 0 on success
 */
extern "C"
VXIjsiResult SBjsiPopScope(VXIjsiInterface        *pThis,
			   VXIjsiContext          *context)
{
  static const wchar_t func[] = L"SBjsiPopScope";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, L"entering: 0x%p", context);

  rc = context->jsiContext->PopScope();

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}


/**
 * Reset the scope chain to the global scope (pop all nested scopes)
 *
 * @param context  [IN] JavaScript context to pop the scopes from
 *
 * @result VXIjsiResult 0 on success
 */
extern "C"
VXIjsiResult SBjsiClearScopes(VXIjsiInterface        *pThis,
			      VXIjsiContext          *context)
{
  static const wchar_t func[] = L"SBjsiClearScopes";
  GET_SBJSI(pThis, context, rc);
  context->jsiContext->Diag(SBJSI_LOG_API, func, L"entering: 0x%p", context);

  rc = context->jsiContext->ClearScopes();

  context->jsiContext->Diag(SBJSI_LOG_API, func, L"exiting: returned %d", rc);
  return rc;
}

extern "C" const VXIMap *SBjsiGetLastError(VXIjsiInterface *pThis, VXIjsiContext *context)
{
  return context->jsiContext->GetLastException();
}
