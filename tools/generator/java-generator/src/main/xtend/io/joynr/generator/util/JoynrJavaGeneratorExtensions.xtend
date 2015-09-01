package io.joynr.generator.util
/*
 * !!!
 *
 * Copyright (C) 2011 - 2015 BMW Car IT GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.util.Iterator
import java.util.TreeSet
import org.franca.core.franca.FAnnotation
import org.franca.core.franca.FAnnotationType
import org.franca.core.franca.FBroadcast
import org.franca.core.franca.FCompoundType
import org.franca.core.franca.FInterface
import org.franca.core.franca.FModelElement
import org.franca.core.franca.FType
import org.franca.core.franca.FMethod
import io.joynr.generator.util.JavaTypeUtil
import com.google.inject.Inject
import java.util.HashMap

class JoynrJavaGeneratorExtensions extends JoynrGeneratorExtensions {
	@Inject extension JavaTypeUtil

	def buildPackagePath(FType datatype, String separator, boolean includeTypeCollection) {
		if (datatype == null) {
			return "";
		}
		var packagepath = "";
		try {
			packagepath = getPackagePathWithJoynrPrefix(datatype, separator);
		} catch (IllegalStateException e){
			//	if an illegal StateException has been thrown, we tried to get the package for a primitive type, so the packagepath stays empty.
		}
		if (packagepath!="") {
			if (includeTypeCollection && datatype.partOfTypeCollection) {
				packagepath += separator + datatype.typeCollectionName;
			}
		};
		return packagepath;
	}

	def String getNamespaceStarter(FInterface interfaceType) {
		getNamespaceStarter(getPackageNames(interfaceType));
	}

	def String getNamespaceStarter(FType datatype) {
		getNamespaceStarter(getPackageNames(datatype));
	}

	def String getNamespaceEnder(FInterface interfaceType) {
		getNamespaceEnder(getPackageNames(interfaceType));
	}

	def String getNamespaceEnder(FType datatype) {
		getNamespaceEnder(getPackageNames(datatype));
	}

	def boolean hasReadAttribute(FInterface interfaceType){
		for(attribute: interfaceType.attributes){
			if (isReadable(attribute)){
				return true
			}
		}
		return false
	}

	def boolean hasWriteAttribute(FInterface interfaceType){
		for(attribute: interfaceType.attributes){
			if (isWritable(attribute)){
				return true
			}
		}
		return false
	}

	def boolean hasMethodWithReturnValue(FInterface interfaceType){
		for(method: interfaceType.methods){
			if (!method.outputParameters.empty){
				return true
			}
		}
		return false
	}

	def boolean hasMethodWithoutReturnValue(FInterface interfaceType) {
		for (method: interfaceType.methods) {
			if (method.outputParameters.empty) {
				return true
			}
		}
		return false
	}

	def boolean hasMethodWithImplicitErrorEnum(FInterface interfaceType){
		for(method: interfaceType.methods){
			if (method.errors != null) {
				return true
			}
		}
		return false
	}

	def boolean hasMethodWithErrorEnum(FInterface interfaceType) {
		for (method : interfaceType.methods) {
			if (method.errorEnum != null) {
				return true;
			}
		}
		return hasMethodWithImplicitErrorEnum(interfaceType);
	}

	def boolean hasErrorEnum(FMethod method) {
		return (method.errors != null) || (method.errorEnum != null);
	}

	def methodToErrorEnumName(FInterface serviceInterface) {
		var HashMap<FMethod, String> methodToErrorEnumName = new HashMap<FMethod, String>()
		var uniqueMethodSignatureToErrorEnumName = new HashMap<String, String>();
		var methodNameToCount = overloadedMethodCounts(getMethods(serviceInterface));
		var methodNameToIndex = new HashMap<String, Integer>();

		for (FMethod method : getMethods(serviceInterface)) {
			if (methodNameToCount.get(method.name) == 1) {
				// method not overloaded, so no index needed
				methodToErrorEnumName.put(method, method.name.toFirstUpper + "ErrorEnum");
			} else {
				// initialize index if not existent
				if (!methodNameToIndex.containsKey(method.name)) {
					methodNameToIndex.put(method.name, 0);
				}
				val methodSignature = createMethodSignatureFromInParameters(method);
				if (!uniqueMethodSignatureToErrorEnumName.containsKey(methodSignature)) {
					var Integer index = methodNameToIndex.get(method.name);
					index++;
					methodNameToIndex.put(method.name, index);
					uniqueMethodSignatureToErrorEnumName.put(methodSignature, method.name.toFirstUpper + index);
				}
				methodToErrorEnumName.put(method, uniqueMethodSignatureToErrorEnumName.get(methodSignature) + "ErrorEnum");
			}
		}
		return methodToErrorEnumName
	}

	def boolean hasMethodWithArguments(FInterface interfaceType){
		for(method: interfaceType.methods){
			if (getInputParameters(method).size>0){
				return true
			}
		}
		return false
	}

	def private String getNamespaceStarter(Iterator<String> packageList){
		return getNamespaceStarterFromPackageList(packageList);
	}

	def String getNamespaceStarterFromPackageList(Iterator<String> packageList){
		var sb = new StringBuilder();
		while(packageList.hasNext){
			sb.append("namespace " + packageList.next + "{ " );
		}
		return sb.toString();
	}

	def private String getNamespaceEnder(Iterator<String> packageList){
		return getNameSpaceEnderFromPackageList(packageList);
	}

	def String getNameSpaceEnderFromPackageList(Iterator<String> packageList){
		var sb = new StringBuilder();
		while(packageList.hasNext){
			sb.insert(0, "} /* namespace " + packageList.next + " */ " );
		}
		return sb.toString();
	}

	def Iterable<String> getRequiredIncludesFor(FCompoundType datatype){
		getRequiredIncludesFor(datatype, true);
	}

	def Iterable<String> getRequiredIncludesFor(FCompoundType datatype, boolean includingExendedType){
		val members = getComplexAndEnumMembers(datatype);

		val typeList = new TreeSet<String>();
		if (hasExtendsDeclaration(datatype)){
			if (includingExendedType){
				typeList.add(getIncludeOf(getExtendedType(datatype)))
			}

			typeList.addAll(getRequiredIncludesFor(getExtendedType(datatype), false))
		}

		for (member : members) {
			val type = getDatatype(member.type);
			if (type instanceof FType){
				typeList.add(getIncludeOf(type));
			}
		}
		return typeList;
	}

	def Iterable<String> getRequiredIncludesFor(FInterface serviceInterface) {
		getRequiredIncludesFor(serviceInterface, true, true, true, true, true);
	}

	def Iterable<String> getRequiredIncludesFor(
			FInterface serviceInterface,
			boolean methods,
			boolean readAttributes,
			boolean writeAttributes,
			boolean notifyAttributes,
			boolean broadcasts
	) {
		val includeSet = new TreeSet<String>();
		for(datatype : getAllComplexAndEnumTypes(serviceInterface, methods, readAttributes, writeAttributes, notifyAttributes, broadcasts)) {
			if (datatype instanceof FType){
				includeSet.add(getIncludeOf(datatype));
			}
//			else{
//				includeSet.add(getIncludeOf(datatype as FBasicTypeId));
//			}
		}
		return includeSet;
	}

	def Iterable<String> getRequiredIncludesFor(FBroadcast broadcast) {
		val includeSet = new TreeSet<String>();
		for(datatype: getAllComplexAndEnumTypes(broadcast)) {
			if (datatype instanceof FType) {
				includeSet.add(getIncludeOf(datatype));
			}
		}
		return includeSet;
	}

	def ReformatComment(FAnnotation comment, String prefixForNewLines) {
		return comment.comment.replaceAll("\\s+", " ").replaceAll("\n", "\n" + prefixForNewLines)
	}

	// for classes and methods
	def appendJavadocSummaryAndWriteSeeAndDescription(FModelElement element, String prefixForNewLines)'''
		«IF element.comment != null»
			«FOR comment : element.comment.elements»
				«IF comment.type == FAnnotationType::DESCRIPTION»
					«prefixForNewLines» «ReformatComment(comment, prefixForNewLines)»
				«ENDIF»
			«ENDFOR»
			«FOR comment : element.comment.elements»
				«IF comment.type == FAnnotationType::SEE»
					«prefixForNewLines» @see «ReformatComment(comment, prefixForNewLines)»
				«ENDIF»
				«IF comment.type == FAnnotationType::DETAILS»
					«prefixForNewLines»
					«prefixForNewLines» «ReformatComment(comment, prefixForNewLines)»
				«ENDIF»
			«ENDFOR»
		«ENDIF»
	'''

	// for parts
	def appendJavadocComment(FModelElement element, String prefixForNewLines)'''
		«IF element.comment != null»
			«FOR comment : element.comment.elements»
				«IF comment.type == FAnnotationType::DESCRIPTION»
					«ReformatComment(comment, prefixForNewLines)»
				«ENDIF»
			«ENDFOR»
		«ENDIF»
	'''

	// for parameters
	def appendJavadocParameter(FModelElement element, String prefixForNewLines)'''
		«IF element.comment != null»
			«FOR comment : element.comment.elements»
				«IF comment.type == FAnnotationType::DESCRIPTION»
					«prefixForNewLines» @param «element.joynrName» «ReformatComment(comment, prefixForNewLines)»
				«ENDIF»
			«ENDFOR»
		«ENDIF»
	'''

	def String getIncludeOf(FType dataType) {
		return dataType.buildPackagePath(".", true) + "." + dataType.joynrName;
	}

	override String getOneLineWarning() {
		//return ""
		return "/* Generated Code */  "
	}

	// Returns true if a class or superclass has array members
	def boolean hasArrayMembers(FCompoundType datatype){
		for (member : datatype.members) {
			if (isArray(member)){
				return true
			}
		}
		// Check any super classes
		if (hasExtendsDeclaration(datatype)) {
			return hasArrayMembers(datatype.extendedType)
		}
		return false
	}

	// Returns true if a class has to create lists in its constructor
	def boolean hasListsInConstructor(FCompoundType datatype){
		for (member : datatype.members) {
			if (isArray(member)){
				return true
			}
		}
		return false
	}

	def getJoynTypePackagePrefix(){
		joynrGenerationPrefix
	}
}