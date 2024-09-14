#pragma once

#include "error.hpp"
#include "io_types.hpp"
#include "io_text_string.hpp"
#include "io_net_ip.hpp"
#include "io_file.hpp"
#include <krb5.h>
#undef TRUE
#undef FALSE

namespace el1::security::kerberos
{
	using namespace io::text::string;
	using namespace io::net::ip;
	using namespace io::file;
	using namespace system::time;
	class TKerberosContext;
	class TKerberosTicketCache;
	class TKerberosCredentials;
	class TKerberoyKeytab;
	class TKerberosTicket;
	class TKerberosAuthenticationContext;

	struct TKerberosException : error::IException
	{
		const krb5_error_code code;
		const TString message;

		TString Message() const;
		IException* Clone() const;

		TKerberosTicketCache& DefaultTicketCache() EL_GETTER;
		void DefaultTicketCache(TKerberosTicketCache*) EL_SETTER;

		TKerberoyKeytab& DefaultKeytab() EL_GETTER;

		std::shared_ptr<TKerberosTicketCache> OpenCache(const TString& name, const bool create);

		TKerberosException(const TKerberosContext& context, const krb5_error_code code);
	};

	#define KRB_ERROR(ctx, expr) do { krb5_error_code code; EL_ERROR( (code = ((expr))) != 0, TKerberosException, ctx, code); } while(false)

	class TKerberosContext
	{
		public:
			krb5_context context;

			TString DefaultRealm() const EL_GETTER;
			void DefaultRealm(const TString&) EL_SETTER;

			TKerberosContext();
			~TKerberosContext();
	};

	/**
	* @enum EPrincipalType
	* @brief Enumerates the types of Kerberos principals, specifying their format and interpretation.
	*/
	enum class EPrincipalType
	{
		/**
		* @brief UNKNOWN type.
		* Used when the type of the principal is not explicitly known or defined.
		* Example: "user@EXAMPLE.COM" where the specific type is not assigned.
		*/
		UNKNOWN = 0,

		/**
		* @brief Standard (PRINCIPAL).
		* Represents a typical principal with an optional instance, commonly used for users or services.
		* Example: "john.doe@EXAMPLE.COM"
		*/
		PRINCIPAL = 1,

		/**
		* @brief Service-instance (SRV_INST).
		* Used for principals that represent a service combined with an instance for uniqueness.
		* The service-instance type (KRB5_NT_SRV_INST) is used for service principals where the principal name includes
		* both the name of the service and a specific instance name, which does not necessarily have to be a hostname.
		* The instance part here can be any string that helps to uniquely identify the service instance within the realm.
		* This type is versatile for applications where services might be distinguished by roles or other identifiers that
		* are not tied to a specific host.
		*
		* Example: ftp/backupserver@EXAMPLE.COM
		* Service: ftp
		* Instance: backupserver (denotes a specific server or role within the service)
		*/
		SRV_INST = 2,

		/**
		* @brief Service-host (SRV_HOST).
		* Used for service principals where the instance is a hostname.
		* The service-host type (KRB5_NT_SRV_HST) specifically expects the instance to be a hostname.
		* This type is tailored for services where it is crucial to specify the host where the service is running, often
		* used in environments where services are distributed across multiple machines.
		*
		* Example: http/www.example.com@EXAMPLE.COM
		* Service: http
		* Instance: www.example.com (specifically denotes the host machine)
		*/
		SRV_HOST = 3,

		/**
		* @brief Extended service-host (SRV_XHOST).
		* Similar to SRV_HOST but includes additional specificity for unique identification.
		*
		* The purpose of using SRV_XHOST is to accommodate scenarios where merely specifying a service type and a
		* hostname is insufficient to uniquely identify a service due to the complexity of the environment.
		* This can include details like:
		*
		* 	Subdomains or service paths: Useful in large organizations or cloud environments where services are
		* 		segmented further than just by the host.
		*
		* 	Port numbers: When multiple instances of the same service run on different ports of the same machine.
		*
		* 	Geographical or organizational qualifiers: These might denote specific data centers or organizational
		* 		units where the service is located.
		*
		*	Service roles or functions: When the same service performs different roles depending on context.
		*/
		SRV_XHOST = 4,

		/**
		* @brief Unique identifier (UID).
		* Intended for use with principals that are defined by unique identifiers rather than names.
		* Example: "UID:123456@EXAMPLE.COM"
		*/
		UID = 5,

		/**
		* @brief Enterprise (ENTERPRISE).
		* Used in enterprise environments where principals might be specified as an email address or similar identifier.
		* Example: "user@example.com@EXAMPLE.COM" - often used in Active Directory environments.
		*/
		ENTERPRISE = 10,

		/**
		* @brief Microsoft-specific (MS_PRINCIPAL).
		* Ensures compatibility with Microsoft's Kerberos implementations.
		* Example: "administrator@EXAMPLE.COM" - with MS-specific encoding or requirements.
		*/
		MS_PRINCIPAL = 11,

		/**
		* @brief Microsoft principal with ID (MS_PRINCIPAL_AND_ID).
		* Microsoft-specific type that includes a unique identifier.
		* Example: "administrator#1234@EXAMPLE.COM" - Microsoft specific with an ID.
		*/
		MS_PRINCIPAL_AND_ID = 12,

		/**
		* @brief Enterprise principal with ID (ENT_PRINCIPAL_AND_ID).
		* Combines an enterprise identifier with a unique ID for complex setups.
		* Example: "user@example.com#5678@EXAMPLE.COM" - Combines a user email with a unique identifier.
		*/
		ENT_PRINCIPAL_AND_ID = 13
	};

	class TKerberosPrincipal
	{
		public:
			const TKerberosContext* const context;
			krb5_principal principal;

			EPrincipalType Type() const EL_GETTER;
			TString Realm() const EL_GETTER;
			TString PrimaryName() const EL_GETTER;
			TString InstanceName() const EL_GETTER;
			TList<TString> Components() const EL_GETTER;

			bool operator==(const TKerberosPrincipal& rhs) const EL_GETTER;
			bool operator!=(const TKerberosPrincipal& rhs) const EL_GETTER;

			/**
			* @brief Constructs a TKerberosPrincipal object from common components.
			*
			* This constructor initializes a Kerberos principal with a specific realm, primary name,
			* and instance name using the provided Kerberos context. If no instance name is required an
			* empty string can be passed instead.
			*
			* @param context Pointer to the TKerberosContext object, which manages the Kerberos environment settings.
			*
			* @param type The principal type that determines the interpretation of the provided values and purpose of the principal.
			*
			* @param realm The realm in which the principal resides, typically representing the administrative domain.
			*              Example: "EXAMPLE.COM"
			*
			* @param primary_name The primary component of the principal name, identifying the user or service.
			*                     Example for a user: "john"
			*                     Example for a service: "http"
			*
			* @param instance_name An optional component used to further specify the primary name, often used for services
			*                      to denote the host or specific role.
			*                      Example for a user (optional role): "admin"
			*                      Example for a service (host): "www.example.com"
			*/
			TKerberosPrincipal(TKerberosContext* const context, const EPrincipalType type, const TString& realm, const TString& primary_name, const TString& instance_name);

			/**
			* @brief Constructs a TKerberosPrincipal object from an abitrary number of components.
			*
			* This constructor initializes a Kerberos principal within the given Kerberos context, setting
			* the principal type and organizing its name components according to the specified order.
			* This is most usefull for the SRV_XHOST type of principal, but can be used with any principal type.
			*
			* @param context Pointer to the TKerberosContext object, which manages the Kerberos environment settings.
			*
			* @param type The type of the principal (e.g. SRV_XHOST) that dictates how the components are interpreted.
			*
			* @param realm The realm in which the principal resides, typically representing the administrative domain.
			*
			* @param components A list of string components that make up the principal's name. The list must be ordered correctly
			*                   according to the principal type's requirements. For example, for SRV_XHOST, the order might be
			*                   service type, followed by hostname, followed by a port number or other identifiers.
			*/
			TKerberosPrincipal(TKerberosContext* const context, const EPrincipalType type, const TString& realm, const TList<TString>& components);

			/**
			* @brief Constructs a TKerberosPrincipal object from a full name.
			*
			* This constructor initializes a Kerberos principal using a provided context,
			* a specified type, and a full principal name. The full principal name should
			* be formatted appropriately according to Kerberos standards, typically in the
			* format 'primary/instance@REALM' for service principals or 'primary@REALM' for
			* user principals. The principal type should align with how the components are
			* expected to be parsed and managed within the Kerberos system.
			*
			* This function should be avoided for the more complex principal types (e.g. SRV_XHOST)
			* as it may fail to correctly identify and break down all components.
			*
			* @param context Pointer to the TKerberosContext object, which manages the Kerberos environment settings.
			*
			* @param type The type of the principal that dictates how the name is parsed.
			*
			* @param full_name The complete string representation of the principal.
			*/
			TKerberosPrincipal(TKerberosContext* const context, const EPrincipalType type, const TString& full_name);

			TKerberosPrincipal(TKerberosPrincipal&&) = default;
			TKerberosPrincipal(const TKerberosPrincipal&);
			~TKerberosPrincipal();
	};

	/**
	* @enum EKerberosCacheType
	* @brief Enumerates types of Kerberos credential caches, each suited to different use cases.
	*
	* This enum provides a structured way to select and manage Kerberos credential cache types
	* within applications, aiding in configuration and initialization of credential handling.
	*/
	enum class EKerberosCacheType
	{
		/**
		* @brief File-based cache, storing credentials on the filesystem in a single file.
		* Example: "FILE:/tmp/krb5cc_1000" where "1000" is the user's UID.
		*/
		FILE,

		/**
		* @brief Directory-based cache, storing each credential in a separate file within a directory.
		* Example: "DIR:/var/krb5/user_creds"
		*/
		DIR,

		/**
		* @brief Memory-based cache, storing credentials in process memory.
		* Example: "MEMORY:krb5cc_12345"
		*/
		MEMORY,

		/**
		* @brief Keyring-based cache using the session keyring.
		* Example: "KEYRING:session"
		*/
		KEYRING_SESSION,

		/**
		* @brief Keyring-based cache using the user keyring.
		* Example: "KEYRING:user"
		*/
		KEYRING_USER,

		/**
		* @brief Keyring-based cache using the persistent keyring.
		* Example: "KEYRING:persistent:1000" where "1000" is the user's UID.
		*/
		KEYRING_PERSISTENT,

		/**
		* @brief Credential cache managed by the Kerberos Credential Manager (KCM) daemon.
		* Example: "KCM:uid=1000" where "1000" is the user's UID.
		*/
		KCM,

		/**
		* @brief API-based cache, where credentials are managed by an external API.
		* Example: "API:VendorSpecific"
		*/
		API
	};

	/**
	* @class TKerberosTicketCache
	* @brief Manages Kerberos tickets within a specific cache context.
	*
	* This class provides an interface for managing a Kerberos ticket cache. It allows
	* for storing, retrieving, removing, and managing the validity and renewal of Kerberos
	* credentials (tickets) associated with different principals.
	*/
	class TKerberosTicketCache
	{
		public:
			const TKerberosContext* const context;
			krb5_ccache cache;

			/**
			* @brief Retrieves the full identifier of the credential cache including the type (residual).
			* @return TString representing the full identifier of the credential cache.
			*/
			TString URL() const EL_GETTER;

			/**
			* @brief Retrieves only the type (residual) of the cache indetifier.
			* @return The EKerberosCacheType describing the nature of the cache.
			*/
			EKerberosCacheType Type() const EL_GETTER;

			/**
			* @brief Retrieves only the file-system path of a cache.
			*        This function is only available for cache types that store their data in the file-system.
			*        When using it on a unsupported cache type the function will throw an error.
			* @return TPath representing the path of the credential cache.
			*         For the FILE type this will point directly to the file that holds the data.
			*         For the DIR type this will point to the root directory of the cache.
			*         For the API type the behavior/support is implementation defined.
			*/
			TPath Path() const EL_GETTER;

			/**
			* @brief Gets a list of all principals whose credentials are stored in the cache.
			* @return TList of TKerberosPrincipal representing all the principals in the cache.
			*/
			TList<TKerberosPrincipal> Content() const EL_GETTER;

			/**
			* @brief Stores a set of credentials in the ticket cache.
			* @param creds Reference to the TKerberosCredentials to be stored in the cache.
			* @param replace Defines whether or not the function is allowed to replace existing
			*                credentials already in the cache.
			*/
			void Store(const TKerberosCredentials& creds, const bool replace);

			/**
			* @brief Retrieves credentials for a specific principal from the cache.
			* @param principal The TKerberosPrincipal for which credentials are to be retrieved.
			* @return TKerberosCredentials object containing the retrieved credentials.
			*/
			TKerberosCredentials Retrieve(const TKerberosPrincipal& principal) const EL_GETTER;

			/**
			* @brief Checks if the cache contains credentials for a specified principal.
			* @param principal The TKerberosPrincipal to check in the cache.
			* @return bool indicating whether the principal's credentials are present in the cache.
			*/
			bool Contains(const TKerberosPrincipal& principal) const EL_GETTER;

			/**
			* @brief Removes credentials of a specific principal from the cache.
			* @param principal The TKerberosPrincipal whose credentials are to be removed.
			* @return bool indicating success of the removal operation.
			*/
			bool Remove(const TKerberosPrincipal& principal);

			/**
			* @brief Renews the credentials for a specified principal.
			*        This function is used to renew credentials that are close to expiration but
			*        still within their renewable period. Renewing a ticket extends its lifetime,
			*        allowing continued use without requiring re-authentication from the user.
			* @param principal The TKerberosPrincipal whose credentials are to be renewed.
			*/
			void Renew(const TKerberosPrincipal& principal);

			/**
			* @brief Validates the credentials for a specified principal.
			*        This function is used to validate a ticket that has been issued as postdated.
			*        Postdated tickets are issued with a start time in the future, and they are not
			*        valid until they have been validated. This process involves contacting the
			*        Key Distribution Center (KDC) to turn the postdated ticket into a valid
			*        service-granting ticket.
			* @param principal The TKerberosPrincipal whose credentials are to be validated.
			*/
			void Validate(const TKerberosPrincipal& principal);

			/**
			* @brief Attempts to renew the credentials for a specified principal.
			* @param principal The TKerberosPrincipal whose credentials are to be renewed.
			* @return bool indicating whether the renewal was successful.
			*/
			bool TryRenew(const TKerberosPrincipal& principal) EL_WARN_UNUSED_RESULT;

			/**
			* @brief Attempts to validate the credentials for a specified principal.
			* @param principal The TKerberosPrincipal whose credentials are to be validated.
			* @return bool indicating whether the validation was successful.
			*/
			bool TryValidate(const TKerberosPrincipal& principal) EL_WARN_UNUSED_RESULT;

			/**
			* @brief Constructs a new ticket cache object.
			* @param context Pointer to TKerberosContext associated with this cache.
			* @param owner The TKerberosPrincipal who owns this cache.
			* @param url The TString indicating the location or name of the cache.
			* @note Examples of cache urls include:
			*   - FILE: Typically uses a path on the filesystem, e.g., "FILE:/tmp/krb5cc_1000" where "1000" is the user's UID.
			*   - DIR: Utilizes a directory structure where each credential is stored in a separate file within a designated
			*          directory, e.g., "DIR:/var/krb5/user_creds" where credentials are organized by principal name or
			*          other identifiers within the directory.
			*   - MEMORY: Represented by an identifier in memory, not accessible via filesystem paths, e.g., "MEMORY:krb5cc_12345".
			*   - KEYRING: Utilizes the Linux kernel keyring facility, e.g., "KEYRING:persistent:1000" where "1000" is the user's UID.
			*   - KCM: Utilizes the Kerberos Credential Manager daemon, e.g., "KCM:uid=1000" where "1000" is the user's UID.
			*   - API: Represents credentials managed by an external API, potentially not exposing a traditional path or identifier, e.g., "API:VendorSpecific".
			*/
			TKerberosTicketCache(TKerberosContext* const context, const TKerberosPrincipal& owner, const TString& url);

			~TKerberosTicketCache();
	};

	/**
	* @class TKerberosCredentials
	* @brief Represents Kerberos credentials within a specified context.
	*
	* This class encapsulates Kerberos credentials, providing methods to check various properties
	* of the credentials such as validity, renewability, and capability flags like forwardable or proxiable.
	* It handles both the creation and management of these credentials within a given Kerberos context.
	*/
	class TKerberosCredentials
	{
		public:
			const TKerberosContext* const context;
			krb5_creds creds;

			~TKerberosCredentials();
			TKerberosCredentials(const TKerberosCredentials& other);
			TKerberosCredentials(TKerberosCredentials&& other) = default;

			/**
			* @brief Constructs Kerberos credentials with specific krb5_creds data.
			* @param context Pointer to a constant TKerberosContext, providing the necessary Kerberos operations environment.
			* @param creds krb5_creds structure containing Kerberos ticket information.
			*/
			TKerberosCredentials(TKerberosContext* const context, krb5_creds creds);

			/**
			* @brief Requests new credentials for a specified client and server within a context.
			* @param context Pointer to the TKerberosContext in which the request is made.
			* @param client TKerberosPrincipal representing the client requesting the credentials.
			* @param server TKerberosPrincipal representing the target server for the credentials.
			* @return TKerberosCredentials object populated with the requested credentials.
			*/
			static TKerberosCredentials Request(TKerberosContext* const context, const TKerberosPrincipal& client, const TKerberosPrincipal& server);

			/**
			* @brief Checks if the credentials are currently valid.
			* @return true if the credentials are valid, false otherwise.
			*/
			bool IsValid() const EL_GETTER;

			/**
			* @brief Checks if the credentials were obtained through an initial authentication.
			* @return true if the credentials are initial, false otherwise.
			*/
			bool IsInitial() const EL_GETTER;

			/**
			* @brief Checks if the credentials are renewable.
			* @return true if the credentials can be renewed, false otherwise.
			*/
			bool IsRenewable() const EL_GETTER;

			/**
			* @brief Checks if the credentials are forwardable.
			* @return true if the credentials can be forwarded, false otherwise.
			*/
			bool IsForwardable() const EL_GETTER;

			/**
			* @brief Checks if the credentials are proxiable.
			* @return true if the credentials can be proxied, false otherwise.
			*/
			bool IsProxiable() const EL_GETTER;

			/**
			* @brief Checks if the credentials support user-to-user authentication.
			* @return true if the credentials are for user-to-user authentication, false otherwise.
			*/
			bool IsUserToUser() const EL_GETTER;

			/**
			* @brief Retrieves the client principal of the credentials.
			* @return Reference to a TKerberosPrincipal representing the client.
			*/
			const TKerberosPrincipal& Client() const EL_GETTER;

			/**
			* @brief Retrieves the server principal of the credentials.
			* @return Reference to a TKerberosPrincipal representing the server.
			*/
			const TKerberosPrincipal& Server() const EL_GETTER;

			/**
			* @brief Retrieves the network addresses associated with the credentials.
			* @return List of ipaddr_t representing the valid addresses for the credentials.
			*/
			TList<ipaddr_t> Addresses() const EL_GETTER;

			/**
			* @brief Retrieves the start time of the credential's validity.
			* @return TTime representing when the credentials become valid.
			*/
			TTime ValidFrom() const EL_GETTER;

			/**
			* @brief Retrieves the end time of the credential's validity.
			* @return TTime representing when the credentials expire.
			*/
			TTime ValidUntil() const EL_GETTER;

			/**
			* @brief Retrieves the latest time until which the credentials can be renewed.
			* @return TTime representing the renew-until time.
			*/
			TTime RenewUntil() const EL_GETTER;
	};

	/**
	* @enum EKerberosKeytabType
	* @brief Enumerates different types of Kerberos keytab storage mechanisms.
	*
	* This enum class provides identifiers for various backend types that store Kerberos key materials,
	* facilitating different security, performance, and management needs across diverse environments.
	*/
	enum class EKerberosKeytabType
	{
		/**
		* @brief File-based keytab, storing keys in a binary file on disk.
		* Example: "FILE:/etc/krb5.keytab"
		*/
		FILE,

		/**
		* @brief Memory-based keytab, storing keys only in volatile memory.
		* Example: "MEMORY:keytab"
		*/
		MEMORY,

		/**
		* @brief Directory-based keytab, storing each key in a separate file within a directory.
		* Example: "DIR:/var/krb5/keytabs/"
		*/
		DIR,

		/**
		* @brief KCM (Kerberos Credential Manager), managing keys in a daemon-based cache.
		* Example: "KCM:uid=1000"
		*/
		KCM,

		/**
		* @brief API-managed keytab, where keys are fetched from a central service via an API.
		* Example: "API:VendorSpecific"
		*/
		API,

		/**
		* @brief Keytab stored using the Linux kernel's keyring facility, linked to a process.
		* Example: "KEYRING:process"
		*/
		KEYRING_PROCESS,

		/**
		* @brief Keytab stored using the Linux kernel's keyring facility, linked to a session.
		* Example: "KEYRING:session"
		*/
		KEYRING_SESSION,

		/**
		* @brief Keytab stored using the Linux kernel's keyring facility, linked to a user.
		* Example: "KEYRING:user"
		*/
		KEYRING_USER,

		/**
		* @brief Keytab stored using the Linux kernel's keyring facility, persistent across reboots.
		* Example: "KEYRING:persistent"
		*/
		KEYRING_PERSISTENT,

		/**
		* @brief MSLSA keytab, utilizing the Microsoft LSA Server's security mechanisms on Windows platforms.
		* Example: "MSLSA:"
		*/
		MSLSA
	};

	/**
	* @enum EKerberosEncType
	* @brief Enumerates supported Kerberos encryption types.
	*
	* This enum class provides identifiers for various encryption types supported by Kerberos.
	* These types define the encryption algorithms used for protecting Kerberos tickets and other sensitive data.
	*/
	enum class EKerberosEncType {
		/**
		* @brief Special value only used for retrieving keys.
		*/
		ANY = 0,

		/**
		* @brief DES CBC mode with CRC-32.
		* @deprecated This encryption type is considered weak and should not be used in new systems.
		*/
		DES_CBC_CRC = 1,

		/**
		* @brief DES CBC mode with MD4.
		* @deprecated This encryption type is considered weak and should not be used in new systems.
		*/
		DES_CBC_MD4 = 2,

		/**
		* @brief DES CBC mode with MD5.
		* @deprecated This encryption type is considered weak and should not be used in new systems.
		*/
		DES_CBC_MD5 = 3,

		/**
		* @brief DES3 CBC mode with SHA1.
		*        Commonly used in older systems for compatibility.
		*/
		DES3_CBC_SHA1 = 16,

		/**
		* @brief AES128 CTS mode with HMAC SHA1-96.
		*        Recommended for current use in Kerberos environments.
		*/
		AES128_CTS_HMAC_SHA1_96 = 17,

		/**
		* @brief AES256 CTS mode with HMAC SHA1-96.
		*        Recommended for high security environments requiring strong encryption.
		*/
		AES256_CTS_HMAC_SHA1_96 = 18,

		/**
		* @brief RC4 HMAC.
		* @deprecated Historically used in Windows environments; known vulnerabilities make it less desirable.
		*/
		RC4_HMAC = 23,

		/**
		* @brief Camellia128 CTS mode with CMAC.
		*        Used in environments looking for an alternative to AES.
		*/
		CAMELLIA128_CTS_CMAC = 25,

		/**
		* @brief Camellia256 CTS mode with CMAC.
		*        Strong encryption alternative to AES, used in high security applications.
		*/
		CAMELLIA256_CTS_CMAC = 26
	};

	/**
	* @class TKerberoyKeytab
	* @brief Manages operations on a Kerberos keytab, such as adding, removing, or retrieving keys.
	*
	* This class provides an interface to interact with a Kerberos keytab. It facilitates the addition
	* and removal of keytab entries and querying of entries based on specific criteria.
	*/
	class TKerberoyKeytab
	{
		public:
			/**
			* @class TEntry
			* @brief Represents an entry within a Kerberos keytab.
			*
			* Encapsulates a single keytab entry, allowing access to principal information, timestamp,
			* version number, and encryption type.
			*/
			class TEntry
			{
				public:
					krb5_keytab_entry entry;

					/**
					* @brief Retrieves the principal associated with this keytab entry.
					* @return TKerberosPrincipal representing the principal.
					*/
					TKerberosPrincipal Principal() const EL_GETTER;

					/**
					* @brief Retrieves the timestamp when this entry was added to the keytab.
					* @return TTime representing the addition timestamp.
					*/
					TTime Timestamp() const EL_GETTER;

					/**
					* @brief Retrieves the version number of this keytab entry.
					* @return krb5_kvno representing the key version number.
					*/
					krb5_kvno Version() const EL_GETTER;

					/**
					* @brief Retrieves the encryption type of the key in this entry.
					* @return EKerberosEncType indicating the encryption type.
					*/
					EKerberosEncType Type() const EL_GETTER;
			};

			const TKerberosContext* const context;
			krb5_keytab keytab;

			/**
			* @brief Retrieves the URL or path of the keytab.
			* @return TString containing the URL or path.
			*         e.g. "FILE:/etc/krb5.keytab"
			*/
			TString URL() const EL_GETTER;

			/**
			* @brief Retrieves the filesystem path of the keytab if applicable.
			* @return TPath containing the filesystem path.
			*         e.g. "/etc/krb5.keytab"
			*/
			TPath Path() const EL_GETTER;

			/**
			* @brief Retrieves the type of the keytab.
			* @return EKerberosKeytabType indicating the type of keytab.
			*/
			EKerberosKeytabType Type() const EL_GETTER;

			/**
			* @brief Adds an entry to the keytab, optionally replacing an existing one.
			* @param entry The entry to add.
			* @param replace Set to true to replace an existing entry if it matches.
			* Example: keytab.Add(entry, true); // Adds or replaces an entry
			*/
			void Add(const TEntry& entry, const bool replace);

			/**
			* @brief Removes entries from the keytab.
			* @param principal The principal of the entry to remove.
			* @param version The version number of the entry to remove.
			* @param type The encryption type of the entry to remove.
			* @return The number of entries removed.
			*/
			unsigned Remove(const TKerberosPrincipal& principal, const krb5_kvno version = 0, const EKerberosEncType type = EKerberosEncType::ANY);

			/**
			* @brief Retrieves a specific entry from the keytab.
			* @param principal The principal of the entry to retrieve.
			* @param version The version number of the entry; specify 0 for the latest.
			* @param type The encryption type of the entry; specify EKerberosEncType::ANY for any type.
			* @return TEntry representing the requested keytab entry.
			* Example: auto entry = keytab.Get(principal, 0, EKerberosEncType::AES256_CTS_HMAC_SHA1_96);
			*/
			TEntry Get(const TKerberosPrincipal& principal, const krb5_kvno version = 0, const EKerberosEncType type = EKerberosEncType::ANY);

			/**
			* @brief Constructs a keytab management object with a specified Kerberos context and URL.
			* @param context Pointer to the TKerberosContext managing this keytab.
			* @param url The URL or path of the keytab to manage.
			*/
			TKerberoyKeytab(TKerberosContext* const context, const TString& url);
			~TKerberoyKeytab();
	};

	class TKerberosKeyblock
	{
		public:
			krb5_keyblock keyblock;
			bool owned;
	};

	class TKerberosTicket
	{
		public:
			const TKerberosContext* const context;
			krb5_ticket ticket;

			~TKerberosTicket();
			TKerberosTicket(const TString& ans1);

			TKerberosPrincipal Principal() const EL_GETTER;
			TKerberosKeyblock SessionKey() const EL_GETTER;
	};

	class TKerberosAuthenticationContext
	{
		public:
			const TKerberosContext* const context;
			krb5_auth_context auth;

			void LocalAddress(ipaddr_t) EL_SETTER;
			ipaddr_t LocalAddress() const EL_GETTER;

			void RemoteAddress(ipaddr_t) EL_SETTER;
			ipaddr_t RemoteAddress() const EL_GETTER;

			TList<byte_t> CreateAuthRequest(const TKerberosPrincipal& service, const TString& hostname, const TList<byte_t>& data);
			bool ProcessAuthRequest(const TList<byte_t>& data);
			TList<byte_t> CreateAuthReply();
			bool ProcessAuthReply(const TList<byte_t>& data);

			TKerberosAuthenticationContext();
			~TKerberosAuthenticationContext();
	};
}
