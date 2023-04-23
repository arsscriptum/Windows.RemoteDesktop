#include "stdafx.h"
#include "Encryption.h"

#include "cryptopp/integer.h"
using CryptoPP::Integer;
#include "cryptopp/osrng.h"
using CryptoPP::AutoSeededRandomPool;
#include "cryptopp/aes.h"
using CryptoPP::AES;
#include "cryptopp/sha.h"
using CryptoPP::SHA256;
#include "cryptopp/gcm.h"
using CryptoPP::GCM;
#include "cryptopp/asn.h"
#include "cryptopp/oids.h"
using CryptoPP::OID;
using namespace CryptoPP::ASN1;

#include "fhmqv.h"
#include "cryptopp/eccrypto.h"
using CryptoPP::ECP;
using CryptoPP::FHMQV;

#include "cryptopp/secblock.h"
using CryptoPP::SecByteBlock;

namespace RemoteDesktop{
	class Encryption_Impl{
	public:

		Encryption_Impl() : AESKey(SHA256::DIGESTSIZE){}
		AutoSeededRandomPool rnd;
		std::unique_ptr<FHMQV<ECP>::Domain> fhmqv;
		SecByteBlock staticprivatekey, staticpublickey, ephemeralprivatekey, ephemeralpublickey, AESKey;
	};
}





RemoteDesktop::Encryption::Encryption()  {
	_Encryption_Impl = std::make_unique<Encryption_Impl>();
}
RemoteDesktop::Encryption::~Encryption() {

}

int RemoteDesktop::Encryption::get_StaticPublicKeyLength() const{
	return  _Encryption_Impl->fhmqv->StaticPublicKeyLength();
}
int RemoteDesktop::Encryption::get_EphemeralPublicKeyLength() const{
	return  _Encryption_Impl->fhmqv->EphemeralPublicKeyLength();
}
const char* RemoteDesktop::Encryption::get_Static_PublicKey() const{
	return (const char*)_Encryption_Impl->staticpublickey.BytePtr();
}
const char* RemoteDesktop::Encryption::get_Ephemeral_PublicKey() const{
	return (const char*)_Encryption_Impl->ephemeralpublickey.BytePtr();
}
void RemoteDesktop::Encryption::set_AES_Key(const char* k){
	_Encryption_Impl->AESKey.resize(SHA256::DIGESTSIZE);
	memcpy(_Encryption_Impl->AESKey.BytePtr(), k, SHA256::DIGESTSIZE);
}

void RemoteDesktop::Encryption::clear(){// clear everything
	clear_keyexchange();
	memset(_Encryption_Impl->AESKey.BytePtr(), 0, _Encryption_Impl->AESKey.SizeInBytes());
	_Encryption_Impl->fhmqv.reset();
}
void RemoteDesktop::Encryption::clear_keyexchange(){// clear only the keys after the initial key  exchange is finished, keep the AES key intact, delete the ECC domain params
	memset(_Encryption_Impl->ephemeralprivatekey.BytePtr(), 0, _Encryption_Impl->ephemeralprivatekey.SizeInBytes());
	memset(_Encryption_Impl->staticprivatekey.BytePtr(), 0, _Encryption_Impl->staticprivatekey.SizeInBytes());
	memset(_Encryption_Impl->staticpublickey.BytePtr(), 0, _Encryption_Impl->staticpublickey.SizeInBytes());
	memset(_Encryption_Impl->ephemeralpublickey.BytePtr(), 0, _Encryption_Impl->ephemeralpublickey.SizeInBytes());
	_Encryption_Impl->fhmqv.reset();
}
void RemoteDesktop::Encryption::Init(bool client){
	clear();// just in case
	const OID CURVE = secp256r1();
	_Encryption_Impl->fhmqv = std::make_unique<FHMQV<ECP>::Domain>(CURVE, client);// allocate here
	_Encryption_Impl->staticprivatekey.resize(_Encryption_Impl->fhmqv->StaticPrivateKeyLength());
	_Encryption_Impl->staticpublickey.resize(_Encryption_Impl->fhmqv->StaticPublicKeyLength());
	_Encryption_Impl->ephemeralprivatekey.resize(_Encryption_Impl->fhmqv->EphemeralPrivateKeyLength());
	_Encryption_Impl->ephemeralpublickey.resize(_Encryption_Impl->fhmqv->EphemeralPublicKeyLength());
	AutoSeededRandomPool rnd;
	_Encryption_Impl->fhmqv->GenerateStaticKeyPair(rnd, _Encryption_Impl->staticprivatekey, _Encryption_Impl->staticpublickey);
	_Encryption_Impl->fhmqv->GenerateEphemeralKeyPair(rnd, _Encryption_Impl->ephemeralprivatekey, _Encryption_Impl->ephemeralpublickey);

}

bool RemoteDesktop::Encryption::Agree(const char *staticOtherPublicKey, const char *ephemeralOtherPublicKey, bool usepreaes){
	SecByteBlock sharedsecret(_Encryption_Impl->fhmqv->AgreedValueLength());

	bool verified = _Encryption_Impl->fhmqv->Agree(sharedsecret, _Encryption_Impl->staticprivatekey, _Encryption_Impl->ephemeralprivatekey, (byte*)staticOtherPublicKey, (byte*)ephemeralOtherPublicKey);
	if (verified){
		if (!usepreaes){
			DEBUG_MSG("Key Exchange Completed . . ");
			Integer ssa;
			ssa.Decode(sharedsecret.BytePtr(), sharedsecret.SizeInBytes());
			SHA256().CalculateDigest(_Encryption_Impl->AESKey, sharedsecret, sharedsecret.size());
		} 
	}
	else DEBUG_MSG("Key Exchange Not Completed . . ");
	clear_keyexchange();
	return verified;

}

bool RemoteDesktop::Encryption::Decrypt(char* in_data, char* out_data, int insize, char* iv){
	size_t multiple = insize / AES::BLOCKSIZE;
	if (multiple * AES::BLOCKSIZE != insize) return false;// data not correctly sized
	GCM<AES>::Decryption Decryptor;
	try{// Crypto++ loves throwing stuff around! If any errors occur, it likely that the peer is messing with us, disconnect 
		Decryptor.SetKeyWithIV(_Encryption_Impl->AESKey, SHA256::DIGESTSIZE, (byte*)iv);
		Decryptor.ProcessData((byte*)out_data, (byte*)in_data, insize);
	}
	catch (CryptoPP::HashVerificationFilter::HashVerificationFailed& e){
		DEBUG_MSG("Caught HashVerificationFailed... %", e.what());
		return false;
	}
	catch (CryptoPP::InvalidArgument& e) {
		DEBUG_MSG("Caught InvalidArgument...%", e.what());
		return false;
	}
	catch (CryptoPP::Exception& e) {
		DEBUG_MSG("Caught Exception...%", e.what());
		return false;
	}
	return true;
}

int RemoteDesktop::Encryption::Ecrypt(char* in_data, char* out_data, int insize, int outsize, char* iv){
	GCM<AES>::Encryption Encryptor;
	try{// Crypto++ loves throwing stuff around! If any errors occur, it likely that something is seriously screwed up on our part
		_Encryption_Impl->rnd.GenerateBlock((byte*)iv, AES::BLOCKSIZE);
		Encryptor.SetKeyWithIV(_Encryption_Impl->AESKey, SHA256::DIGESTSIZE, (byte*)iv);
		auto bytes = roundUp(insize, AES::BLOCKSIZE);
		Encryptor.ProcessData((byte*)out_data, (byte*)in_data, bytes);// number of bytes to encrypt is always 16 less than the length
		assert(outsize >= bytes);
		return bytes;
	}
	catch (CryptoPP::InvalidArgument& e) {
		DEBUG_MSG("Caught InvalidArgument...%", e.what());
		return -1;
	}
	catch (CryptoPP::Exception& e) {
		DEBUG_MSG("Caught Exception...%", e.what());
		return -1;
	}
	return -1;
}